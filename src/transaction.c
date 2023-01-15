#include "splinterdb/transaction.h"
#include "transaction_internal.h"
#include "splinterdb_internal.h"
#include "platform_linux/platform.h"
#include "data_internal.h"
#include "poison.h"
#include "util.h"
#include <stdlib.h>

static int
get_ts_from_splinterdb(const splinterdb *kvsb, slice key, tictoc_timestamp *ts)
{
   splinterdb_lookup_result result;
   splinterdb_lookup_result_init(kvsb, &result, 0, NULL);

   splinterdb_lookup(kvsb, key, &result);

   slice value;

   if (splinterdb_lookup_found(&result)) {
      splinterdb_lookup_result_value(&result, &value);
      memmove(ts, slice_data(value), sizeof(*ts));
   } else {
      *ts = 0;
   }

   splinterdb_lookup_result_deinit(&result);

   return 0;
}

/*
 * Algorithm 1: Read Phase
 */

static int
tictoc_read(transactional_splinterdb *txn_kvsb,
            tictoc_transaction       *tt_txn,
            slice                     user_key,
            splinterdb_lookup_result *result)
{
   int rc = splinterdb_lookup(txn_kvsb->kvsb, user_key, result);

   if (splinterdb_lookup_found(result)) {
      tictoc_rw_entry *r = tictoc_get_new_read_set_entry(tt_txn);
      platform_assert(!tictoc_rw_entry_is_invalid(r));

      slice value;
      splinterdb_lookup_result_value(result, &value);
      writable_buffer_init_from_slice(&r->tuple,
                                      0,
                                      value); // FIXME: use a correct heap_id
      tictoc_rw_entry_set_point_key(
         r, user_key, txn_kvsb->tcfg->kvsb_cfg.data_cfg);

      tictoc_tuple_header       *tuple   = writable_buffer_data(&r->tuple);
      _splinterdb_lookup_result *_result = (_splinterdb_lookup_result *)result;
      uint64 app_value_size = merge_accumulator_length(&_result->value)
                              - sizeof(tictoc_tuple_header);
      memmove(
         merge_accumulator_data(&_result->value), tuple->value, app_value_size);
      merge_accumulator_resize(&_result->value, app_value_size);
   }

   return rc;
}

/*
 * Algorithm 2: Validation Phase
 */
static bool
tictoc_validation(transactional_splinterdb *txn_kvsb,
                  tictoc_transaction       *tt_txn)
{
   for (uint64 i = 0; i < tt_txn->read_cnt; ++i) {
      tictoc_rw_entry *r = tictoc_get_read_set_entry(tt_txn, i);

      slice rkey = writable_buffer_to_slice(&r->key);

      tictoc_timestamp record_tid;
      get_ts_from_splinterdb(txn_kvsb->kvsb, rkey, &record_tid);

      bool is_read_entry_written_by_another =
         get_ts_from_tictoc_rw_entry(r) != record_tid;
      bool is_read_entry_locked_by_another =
         lock_table_is_entry_locked(txn_kvsb->lock_tbl, r)
         && tictoc_rw_entry_is_not_in_write_set(
            tt_txn, r, txn_kvsb->tcfg->kvsb_cfg.data_cfg);

      bool need_to_abort =
         is_read_entry_written_by_another || is_read_entry_locked_by_another;
      if (need_to_abort) {
         return FALSE;
      }

      tt_txn->commit_tid = MAX(tt_txn->commit_tid, record_tid);
   }

   for (uint64 i = 0; i < tt_txn->write_cnt; ++i) {
      tictoc_rw_entry *w    = tictoc_get_write_set_entry(tt_txn, i);
      slice            wkey = writable_buffer_to_slice(&w->key);
      tictoc_timestamp record_tid;
      get_ts_from_splinterdb(txn_kvsb->kvsb, wkey, &record_tid);
      tt_txn->commit_tid = MAX(tt_txn->commit_tid, record_tid);
   }

   return TRUE;
}

/*
 * Algorithm 3: Write Phase
 */
static void
tictoc_write(transactional_splinterdb *txn_kvsb, tictoc_transaction *tt_txn)
{
   const splinterdb *kvsb = txn_kvsb->kvsb;

   for (uint64 i = 0; i < tt_txn->write_cnt; ++i) {
      tictoc_rw_entry *w = tictoc_get_write_set_entry(tt_txn, i);

      slice wkey = writable_buffer_to_slice(&w->key);

      memmove(writable_buffer_data(&w->tuple),
              &tt_txn->commit_tid,
              sizeof(tt_txn->commit_tid));

      int rc = 0;

      // TODO: merge messages in the write set and write to splinterdb
      switch (w->op) {
         case MESSAGE_TYPE_INSERT:
            rc = splinterdb_insert(
               kvsb, wkey, writable_buffer_to_slice(&w->tuple));
            break;
         case MESSAGE_TYPE_UPDATE:
            rc = splinterdb_update(
               kvsb, wkey, writable_buffer_to_slice(&w->tuple));
            break;
         case MESSAGE_TYPE_DELETE:
            rc = splinterdb_delete(kvsb, wkey);
            break;
         default:
            break;
      }

      platform_assert(rc == 0, "Error from SplinterDB: %d\n", rc);

      writable_buffer_deinit(&w->tuple);
   }
}

static int
tictoc_local_write(transactional_splinterdb *txn_kvsb,
                   tictoc_transaction       *txn,
                   tictoc_timestamp          ts,
                   slice                     user_key,
                   message                   msg)
{
   const data_config *cfg =
      txn_kvsb->tcfg->txn_data_cfg->application_data_config;

   // TODO: this part can be done by binary search if the write_set is sorted
   for (uint64 i = 0; i < txn->write_cnt; ++i) {
      tictoc_rw_entry *w = tictoc_get_write_set_entry(txn, i);
      key wkey = key_create_from_slice(writable_buffer_to_slice(&w->key));
      key ukey = key_create_from_slice(user_key);
      if (data_key_compare(cfg, wkey, ukey) == 0) {
         if (message_is_definitive(msg)) {
            w->op = message_class(msg);

            slice value = message_slice(msg);
            writable_buffer_resize(
               &w->tuple, sizeof(tictoc_timestamp) + slice_length(value));

            tictoc_tuple_header *tuple = writable_buffer_data(&w->tuple);

            memmove(&tuple->ts, &ts, sizeof(tictoc_timestamp));
            memmove(tuple->value, slice_data(value), slice_length(value));
         } else {
            platform_assert(w->op != MESSAGE_TYPE_DELETE);

            merge_accumulator new_message;
            merge_accumulator_init_from_message(&new_message, 0, msg);

            tictoc_tuple_header *tuple = writable_buffer_data(&w->tuple);
            slice   old_value   = slice_create(writable_buffer_length(&w->tuple)
                                              - sizeof(tictoc_tuple_header),
                                           tuple->value);
            message old_message = message_create(w->op, old_value);

            data_merge_tuples(cfg, ukey, old_message, &new_message);

            writable_buffer_resize(&w->tuple,
                                   sizeof(tictoc_timestamp)
                                      + merge_accumulator_length(&new_message));

            tuple = writable_buffer_data(&w->tuple);

            memmove(&tuple->ts, &ts, sizeof(tictoc_timestamp));
            memmove(tuple->value,
                    merge_accumulator_data(&new_message),
                    merge_accumulator_length(&new_message));

            merge_accumulator_deinit(&new_message);
         }

         return 0;
      }
   }

   tictoc_rw_entry *w = tictoc_get_new_write_set_entry(txn);
   platform_assert(!tictoc_rw_entry_is_invalid(w));

   w->op = message_class(msg);
   tictoc_rw_entry_set_point_key(
      w, user_key, txn_kvsb->tcfg->kvsb_cfg.data_cfg);

   slice value = message_slice(msg);

   writable_buffer_init(&w->tuple, 0); // FIXME: use a correct heap_id
   writable_buffer_resize(&w->tuple,
                          sizeof(tictoc_timestamp) + slice_length(value));

   tictoc_tuple_header *tuple = writable_buffer_data(&w->tuple);

   memmove(&tuple->ts, &ts, sizeof(tictoc_timestamp));
   memmove(tuple->value, slice_data(value), slice_length(value));

   return 0;
}

static int
transactional_splinterdb_create_or_open(const splinterdb_config   *kvsb_cfg,
                                        transactional_splinterdb **txn_kvsb,
                                        bool open_existing)
{
   transactional_splinterdb_config *txn_splinterdb_cfg;
   txn_splinterdb_cfg = TYPED_ZALLOC(0, txn_splinterdb_cfg);
   memmove(txn_splinterdb_cfg, kvsb_cfg, sizeof(txn_splinterdb_cfg->kvsb_cfg));
   txn_splinterdb_cfg->isol_level = TRANSACTION_ISOLATION_LEVEL_SERIALIZABLE;

   txn_splinterdb_cfg->txn_data_cfg =
      TYPED_ZALLOC(0, txn_splinterdb_cfg->txn_data_cfg);
   transactional_data_config_init(kvsb_cfg->data_cfg,
                                  txn_splinterdb_cfg->txn_data_cfg);

   txn_splinterdb_cfg->kvsb_cfg.data_cfg =
      (data_config *)txn_splinterdb_cfg->txn_data_cfg;

   transactional_splinterdb *_txn_kvsb;
   _txn_kvsb       = TYPED_ZALLOC(0, _txn_kvsb);
   _txn_kvsb->tcfg = txn_splinterdb_cfg;

   int rc = splinterdb_create_or_open(
      &txn_splinterdb_cfg->kvsb_cfg, &_txn_kvsb->kvsb, open_existing);
   bool fail_to_create_splinterdb = (rc != 0);
   if (fail_to_create_splinterdb) {
      platform_free(0, _txn_kvsb);
      platform_free(0, txn_splinterdb_cfg->txn_data_cfg);
      platform_free(0, txn_splinterdb_cfg);
      return rc;
   }

   _txn_kvsb->lock_tbl = lock_table_create();

   hash_lock_init(&_txn_kvsb->hash_lock, &default_hash_lock_config);

   *txn_kvsb = _txn_kvsb;

   return 0;
}

int
transactional_splinterdb_create(const splinterdb_config   *kvsb_cfg,
                                transactional_splinterdb **txn_kvsb)
{
   return transactional_splinterdb_create_or_open(kvsb_cfg, txn_kvsb, FALSE);
}


int
transactional_splinterdb_open(const splinterdb_config   *kvsb_cfg,
                              transactional_splinterdb **txn_kvsb)
{
   return transactional_splinterdb_create_or_open(kvsb_cfg, txn_kvsb, TRUE);
}

void
transactional_splinterdb_close(transactional_splinterdb **txn_kvsb)
{
   transactional_splinterdb *_txn_kvsb = *txn_kvsb;
   splinterdb_close(&_txn_kvsb->kvsb);

   hash_lock_deinit(&_txn_kvsb->hash_lock);

   lock_table_destroy(_txn_kvsb->lock_tbl);

   platform_free(0, _txn_kvsb->tcfg->txn_data_cfg);
   platform_free(0, _txn_kvsb->tcfg);
   platform_free(0, _txn_kvsb);

   *txn_kvsb = NULL;
}

void
transactional_splinterdb_register_thread(transactional_splinterdb *kvs)
{
   splinterdb_register_thread(kvs->kvsb);
}

void
transactional_splinterdb_deregister_thread(transactional_splinterdb *kvs)
{
   splinterdb_deregister_thread(kvs->kvsb);
}

int
transactional_splinterdb_begin(transactional_splinterdb *txn_kvsb,
                               transaction              *txn)
{
   tictoc_transaction_init(&txn->tictoc, txn_kvsb->tcfg->isol_level);
   return 0;
}

int
transactional_splinterdb_commit(transactional_splinterdb *txn_kvsb,
                                transaction              *txn)
{
   tictoc_transaction *tt_txn = &txn->tictoc;

   bool write_successfully = FALSE;

   // Step 1: Lock Write Set
   tictoc_transaction_sort_write_set(
      tt_txn, txn_kvsb->tcfg->txn_data_cfg->application_data_config);

   while (tictoc_transaction_lock_all_write_set(tt_txn, txn_kvsb->lock_tbl)
          == FALSE)
   {
      platform_sleep_ns(
         1000); // 1us is the value that is mentioned in the paper
   }

   if (tictoc_validation(txn_kvsb, &txn->tictoc)) {
      tictoc_write(txn_kvsb, &txn->tictoc);
      write_successfully = TRUE;
   }

   tictoc_transaction_unlock_all_write_set(tt_txn, txn_kvsb->lock_tbl);
   tictoc_transaction_deinit(tt_txn, txn_kvsb->lock_tbl);

   return write_successfully ? 0 : -1;
}

int
transactional_splinterdb_abort(transactional_splinterdb *txn_kvsb,
                               transaction              *txn)
{
   tictoc_transaction_deinit(&txn->tictoc, txn_kvsb->lock_tbl);

   return 0;
}

int
transactional_splinterdb_insert(transactional_splinterdb *txn_kvsb,
                                transaction              *txn,
                                slice                     user_key,
                                slice                     value)
{
   return tictoc_local_write(txn_kvsb,
                             &txn->tictoc,
                             0,
                             user_key,
                             message_create(MESSAGE_TYPE_INSERT, value));
}

int
transactional_splinterdb_delete(transactional_splinterdb *txn_kvsb,
                                transaction              *txn,
                                slice                     user_key)
{
   return tictoc_local_write(
      txn_kvsb, &txn->tictoc, 0, user_key, DELETE_MESSAGE);
}

int
transactional_splinterdb_update(transactional_splinterdb *txn_kvsb,
                                transaction              *txn,
                                slice                     user_key,
                                slice                     delta)
{
   return tictoc_local_write(txn_kvsb,
                             &txn->tictoc,
                             0,
                             user_key,
                             message_create(MESSAGE_TYPE_UPDATE, delta));
}

int
transactional_splinterdb_lookup(transactional_splinterdb *txn_kvsb,
                                transaction              *txn,
                                slice                     user_key,
                                splinterdb_lookup_result *result)
{
   return tictoc_read(txn_kvsb, &txn->tictoc, user_key, result);
}

void
transactional_splinterdb_lookup_result_init(
   transactional_splinterdb *txn_kvsb,   // IN
   splinterdb_lookup_result *result,     // IN/OUT
   uint64                    buffer_len, // IN
   char                     *buffer      // IN
)
{
   return splinterdb_lookup_result_init(
      txn_kvsb->kvsb, result, buffer_len, buffer);
}

void
transactional_splinterdb_set_isolation_level(
   transactional_splinterdb   *txn_kvsb,
   transaction_isolation_level isol_level)
{
   platform_assert(isol_level > TRANSACTION_ISOLATION_LEVEL_INVALID);
   platform_assert(isol_level < TRANSACTION_ISOLATION_LEVEL_MAX_VALID);

   txn_kvsb->tcfg->isol_level = isol_level;
}
