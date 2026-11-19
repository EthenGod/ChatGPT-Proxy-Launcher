use crate::model::{Action, BridgeError, Options, SaveResult, Snapshot};
use serde::de::DeserializeOwned;
use std::{ptr, slice};
use windows_sys::Win32::Foundation::ERROR_INVALID_DATA;

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
impl Buffer {
    fn decode<T: DeserializeOwned>(&self, code: u32) -> Result<T, BridgeError> {
        if self.data.is_null() || self.length == 0 || self.length > MAX_RESPONSE {
            return Err(BridgeError::native(if code == 0 {
                ERROR_INVALID_DATA
            } else {
                code
            }));
        }
        // C++ owns these bytes until this Buffer drops. Deserialization owns its result.
        let data = unsafe { slice::from_raw_parts(self.data, self.length) };
        if code != 0 {
            return Err(serde_json::from_slice(data).unwrap_or_else(|_| BridgeError::native(code)));
        }
        serde_json::from_slice(data).map_err(|_| BridgeError::native(ERROR_INVALID_DATA))
    }
}
unsafe extern "C" {
    fn setup_bridge_initialize();
    fn setup_bridge_action(
        action: u32,
        request: *const u8,
        request_length: usize,
        paths: *const u8,
        paths_length: usize,
        owner: usize,
        output: *mut Buffer,
        commit: *mut u32,
    ) -> u32;
    fn setup_bridge_free(buffer: *mut Buffer);
    fn setup_bridge_read_state(paths: *const u8, length: usize, output: *mut Buffer) -> u32;
    fn setup_bridge_headless(
        tail: *const u16,
        length: usize,
        paths: *const u8,
        paths_length: usize,
    ) -> i32;
    fn setup_bridge_show_bootstrap_error(compatibility_failed: u32);
    fn setup_bridge_compatibility(paths: *const u8, length: usize) -> i32;
}
fn bytes(value: Option<&[u8]>) -> (*const u8, usize) {
    value.map_or((ptr::null(), 0), |v| (v.as_ptr(), v.len()))
}
pub fn initialize() {
    unsafe { setup_bridge_initialize() }
}
pub fn bootstrap_error(compatibility_failed: bool) {
    unsafe { setup_bridge_show_bootstrap_error(u32::from(compatibility_failed)) }
}
pub fn headless(tail: &[u16], paths: Option<&[u8]>) -> i32 {
    let (paths, length) = bytes(paths);
    unsafe { setup_bridge_headless(tail.as_ptr(), tail.len(), paths, length) }
}
pub fn compatibility(paths: Option<&[u8]>) -> i32 {
    let (paths, length) = bytes(paths);
    unsafe { setup_bridge_compatibility(paths, length) }
}
pub fn read_state(paths: Option<&[u8]>) -> Result<Snapshot, BridgeError> {
    let mut buffer = Buffer::default();
    let (paths, length) = bytes(paths);
    let code = unsafe { setup_bridge_read_state(paths, length, &mut buffer) };
    let state: Snapshot = buffer.decode(code)?;
    if state.schema_version != 1 || state.payload_bytes == 0 {
        return Err(BridgeError::native(ERROR_INVALID_DATA));
    }
    Ok(state)
}

pub fn action<T: DeserializeOwned>(
    action: Action,
    request: &impl serde::Serialize,
    paths: Option<&[u8]>,
    owner: usize,
) -> (u32, Result<T, BridgeError>) {
    let mut buffer = Buffer::default();
    let request = match serde_json::to_vec(request) {
        Ok(value) => value,
        Err(_) => return (0, Err(BridgeError::native(ERROR_INVALID_DATA))),
    };
    let (paths, length) = bytes(paths);
    let mut commit = 0;
    let code = unsafe {
        setup_bridge_action(
            action as u32,
            request.as_ptr(),
            request.len(),
            paths,
            length,
            owner,
            &mut buffer,
            &mut commit,
        )
    };
    (commit, buffer.decode(code))
}
pub fn save(options: Options, paths: Option<&[u8]>) -> SaveResult {
    let (commit, result) = action::<SaveResult>(Action::Save, &options, paths, 0);
    result
        .unwrap_or_else(|error| SaveResult::unavailable(commit, error.message, error.native_error))
}
