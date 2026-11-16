use crate::model::{BridgeError, Snapshot};
use std::{ptr, slice};

const MAX_RESPONSE: usize = 1024 * 1024;
#[repr(C)]
struct Buffer {
    data: *mut u8,
    length: usize,
}
impl Default for Buffer {
    fn default() -> Self {
        Self {
            data: ptr::null_mut(),
            length: 0,
        }
    }
}
impl Drop for Buffer {
    fn drop(&mut self) {
        // This allocation always belongs to the C++ bridge, including error replies.
        unsafe { setup_bridge_free(self) }
    }
}
unsafe extern "C" {
    fn setup_bridge_initialize();
    fn setup_bridge_free(buffer: *mut Buffer);
    fn setup_bridge_read_state(paths: *const u8, length: usize, output: *mut Buffer) -> u32;
    fn setup_bridge_headless(
        tail: *const u16,
        length: usize,
        paths: *const u8,
        paths_length: usize,
    ) -> i32;
    fn setup_bridge_show_bootstrap_error(webview_missing: u32);
}
fn bytes(value: Option<&[u8]>) -> (*const u8, usize) {
    value.map_or((ptr::null(), 0), |v| (v.as_ptr(), v.len()))
}
pub fn initialize() {
    unsafe { setup_bridge_initialize() }
}
pub fn bootstrap_error(missing: bool) {
    unsafe { setup_bridge_show_bootstrap_error(u32::from(missing)) }
}
pub fn headless(tail: &[u16], paths: Option<&[u8]>) -> i32 {
    let (paths, length) = bytes(paths);
    unsafe { setup_bridge_headless(tail.as_ptr(), tail.len(), paths, length) }
}
pub fn read_state(paths: Option<&[u8]>) -> Result<Snapshot, BridgeError> {
    let mut buffer = Buffer::default();
    let (paths, length) = bytes(paths);
    let code = unsafe { setup_bridge_read_state(paths, length, &mut buffer) };
    if buffer.data.is_null() || buffer.length == 0 || buffer.length > MAX_RESPONSE {
        return Err(BridgeError::native(if code == 0 { 13 } else { code }));
    }
    let data = unsafe { slice::from_raw_parts(buffer.data, buffer.length) };
    if code != 0 {
        return Err(serde_json::from_slice(data).unwrap_or_else(|_| BridgeError::native(code)));
    }
    let state: Snapshot = serde_json::from_slice(data).map_err(|_| BridgeError::native(13))?;
    if state.schema_version != 1 || state.payload_bytes == 0 {
        return Err(BridgeError::native(13));
    }
    Ok(state)
}
