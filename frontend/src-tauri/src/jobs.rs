use crate::model::BridgeError;
use std::sync::{Arc, Mutex};
use windows_sys::Win32::Foundation::{ERROR_BUSY, ERROR_INVALID_DATA};

#[derive(Default)]
struct Activity {
    pending: usize,
    closing: bool,
    close_listener_ready: bool,
}

pub struct NativeJobs {
    activity: Mutex<Activity>,
    serial: Arc<tokio::sync::Mutex<()>>,
    paths: Option<Vec<u8>>,
}

struct PendingJob(Arc<NativeJobs>);

impl PendingJob {
    fn paths(&self) -> Option<&[u8]> {
        self.0.paths.as_deref()
    }
}

impl Drop for PendingJob {
    fn drop(&mut self) {
        self.0.activity.lock().unwrap().pending -= 1;
    }
}

impl NativeJobs {
    pub fn new(paths: Option<Vec<u8>>) -> Self {
        Self {
            activity: Mutex::default(),
            serial: Arc::default(),
            paths,
        }
    }

    pub fn try_close(&self) -> bool {
        let mut activity = self.activity.lock().unwrap();
        if activity.closing || activity.pending != 0 {
            return false;
        }
        activity.closing = true;
        true
    }

    pub fn reopen(&self) {
        self.activity.lock().unwrap().closing = false;
    }

    pub fn set_close_listener_ready(&self, ready: bool) {
        self.activity.lock().unwrap().close_listener_ready = ready;
    }

    pub fn allow_native_close(&self) -> bool {
        let mut activity = self.activity.lock().unwrap();
        // The page cannot edit settings before the listener handshake succeeds.
        if !activity.close_listener_ready && activity.pending == 0 {
            activity.closing = true;
        }
        activity.closing
    }

    pub async fn run<R, F>(self: &Arc<Self>, work: F) -> Result<R, BridgeError>
    where
        R: Send + 'static,
        F: FnOnce(Option<&[u8]>) -> Result<R, BridgeError> + Send + 'static,
    {
        {
            let mut activity = self.activity.lock().unwrap();
            if activity.closing {
                return Err(BridgeError::native(ERROR_BUSY));
            }
            activity.pending += 1;
        }
        let pending = PendingJob(Arc::clone(self));
        let serial = Arc::clone(&self.serial).lock_owned().await;
        // Once dispatched, the native call owns both guards even if its caller disappears.
        tokio::task::spawn_blocking(move || {
            let _serial = serial;
            work(pending.paths())
        })
        .await
        .map_err(|_| BridgeError {
            commit: "unknown".into(),
            ..BridgeError::native(ERROR_INVALID_DATA)
        })?
    }
}
