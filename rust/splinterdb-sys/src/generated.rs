/* automatically generated by rust-bindgen 0.59.2 */

pub const MAX_KEY_SIZE: u32 = 24;
pub const MAX_MESSAGE_SIZE: u32 = 128;
pub const KVSTORE_BASIC_MIN_KEY_SIZE: u32 = 2;
pub type key_comparator_fn = ::std::option::Option<
    unsafe extern "C" fn(
        context: *const ::std::os::raw::c_void,
        key1: *const ::std::os::raw::c_void,
        key1_len: usize,
        key2: *const ::std::os::raw::c_void,
        key2_len: usize,
    ) -> ::std::os::raw::c_int,
>;
#[repr(C)]
#[derive(Debug)]
pub struct kvstore_basic_cfg {
    pub filename: *const ::std::os::raw::c_char,
    pub cache_size: usize,
    pub disk_size: usize,
    pub max_key_size: usize,
    pub max_value_size: usize,
    pub key_comparator: key_comparator_fn,
    pub key_comparator_context: *mut ::std::os::raw::c_void,
    pub heap_handle: *mut ::std::os::raw::c_void,
    pub heap_id: *mut ::std::os::raw::c_void,
}
#[test]
fn bindgen_test_layout_kvstore_basic_cfg() {
    assert_eq!(
        ::std::mem::size_of::<kvstore_basic_cfg>(),
        72usize,
        concat!("Size of: ", stringify!(kvstore_basic_cfg))
    );
    assert_eq!(
        ::std::mem::align_of::<kvstore_basic_cfg>(),
        8usize,
        concat!("Alignment of ", stringify!(kvstore_basic_cfg))
    );
    assert_eq!(
        unsafe { &(*(::std::ptr::null::<kvstore_basic_cfg>())).filename as *const _ as usize },
        0usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(filename)
        )
    );
    assert_eq!(
        unsafe { &(*(::std::ptr::null::<kvstore_basic_cfg>())).cache_size as *const _ as usize },
        8usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(cache_size)
        )
    );
    assert_eq!(
        unsafe { &(*(::std::ptr::null::<kvstore_basic_cfg>())).disk_size as *const _ as usize },
        16usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(disk_size)
        )
    );
    assert_eq!(
        unsafe { &(*(::std::ptr::null::<kvstore_basic_cfg>())).max_key_size as *const _ as usize },
        24usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(max_key_size)
        )
    );
    assert_eq!(
        unsafe {
            &(*(::std::ptr::null::<kvstore_basic_cfg>())).max_value_size as *const _ as usize
        },
        32usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(max_value_size)
        )
    );
    assert_eq!(
        unsafe {
            &(*(::std::ptr::null::<kvstore_basic_cfg>())).key_comparator as *const _ as usize
        },
        40usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(key_comparator)
        )
    );
    assert_eq!(
        unsafe {
            &(*(::std::ptr::null::<kvstore_basic_cfg>())).key_comparator_context as *const _
                as usize
        },
        48usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(key_comparator_context)
        )
    );
    assert_eq!(
        unsafe { &(*(::std::ptr::null::<kvstore_basic_cfg>())).heap_handle as *const _ as usize },
        56usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(heap_handle)
        )
    );
    assert_eq!(
        unsafe { &(*(::std::ptr::null::<kvstore_basic_cfg>())).heap_id as *const _ as usize },
        64usize,
        concat!(
            "Offset of field: ",
            stringify!(kvstore_basic_cfg),
            "::",
            stringify!(heap_id)
        )
    );
}
#[repr(C)]
#[derive(Debug)]
pub struct kvstore_basic {
    _unused: [u8; 0],
}
extern "C" {
    pub fn kvstore_basic_create(
        cfg: *const kvstore_basic_cfg,
        kvsb: *mut *mut kvstore_basic,
    ) -> ::std::os::raw::c_int;
}
extern "C" {
    pub fn kvstore_basic_open(
        cfg: *const kvstore_basic_cfg,
        kvsb: *mut *mut kvstore_basic,
    ) -> ::std::os::raw::c_int;
}
extern "C" {
    pub fn kvstore_basic_close(kvsb: *mut kvstore_basic);
}
extern "C" {
    pub fn kvstore_basic_register_thread(kvsb: *const kvstore_basic);
}
extern "C" {
    pub fn kvstore_basic_deregister_thread(kvsb: *const kvstore_basic);
}
extern "C" {
    pub fn kvstore_basic_insert(
        kvsb: *const kvstore_basic,
        key: *const ::std::os::raw::c_char,
        key_len: usize,
        value: *const ::std::os::raw::c_char,
        val_len: usize,
    ) -> ::std::os::raw::c_int;
}
extern "C" {
    pub fn kvstore_basic_delete(
        kvsb: *const kvstore_basic,
        key: *const ::std::os::raw::c_char,
        key_len: usize,
    ) -> ::std::os::raw::c_int;
}
extern "C" {
    pub fn kvstore_basic_lookup(
        kvsb: *const kvstore_basic,
        key: *const ::std::os::raw::c_char,
        key_len: usize,
        val: *mut ::std::os::raw::c_char,
        val_max_len: usize,
        val_bytes: *mut usize,
        val_truncated: *mut bool,
        found: *mut bool,
    ) -> ::std::os::raw::c_int;
}
#[repr(C)]
#[derive(Debug)]
pub struct kvstore_basic_iterator {
    _unused: [u8; 0],
}
extern "C" {
    pub fn kvstore_basic_iter_init(
        kvsb: *const kvstore_basic,
        iter: *mut *mut kvstore_basic_iterator,
        start_key: *const ::std::os::raw::c_char,
        start_key_len: usize,
    ) -> ::std::os::raw::c_int;
}
extern "C" {
    pub fn kvstore_basic_iter_deinit(iterpp: *mut *mut kvstore_basic_iterator);
}
extern "C" {
    pub fn kvstore_basic_iter_valid(iter: *mut kvstore_basic_iterator) -> bool;
}
extern "C" {
    pub fn kvstore_basic_iter_next(iter: *mut kvstore_basic_iterator);
}
extern "C" {
    pub fn kvstore_basic_iter_get_current(
        iter: *mut kvstore_basic_iterator,
        key: *mut *const ::std::os::raw::c_char,
        key_len: *mut usize,
        value: *mut *const ::std::os::raw::c_char,
        val_len: *mut usize,
    );
}
extern "C" {
    pub fn kvstore_basic_iter_status(iter: *const kvstore_basic_iterator) -> ::std::os::raw::c_int;
}
extern "C" {
    pub fn kvstore_basic_get_version() -> *const ::std::os::raw::c_char;
}