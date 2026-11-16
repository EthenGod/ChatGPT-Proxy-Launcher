/// Match the Windows executable-token scan, then preserve the remaining UTF-16
/// exactly. Do not round-trip user arguments through UTF-8 or re-quote them.
pub fn command_tail(command: &[u16]) -> &[u16] {
    let mut quoted = false;
    let mut end = 0;
    while end < command.len() {
        let ch = command[end];
        if ch == 34 {
            quoted = !quoted;
        } else if !quoted && (ch == 32 || ch == 9) {
            break;
        }
        end += 1;
    }
    while end < command.len() && (command[end] == 32 || command[end] == 9) {
        end += 1;
    }
    &command[end..]
}

pub fn current_command_tail() -> Vec<u16> {
    unsafe {
        let raw = windows_sys::Win32::System::Environment::GetCommandLineW();
        if raw.is_null() {
            return vec![];
        }
        let mut len = 0;
        while len < 32767 && *raw.add(len) != 0 {
            len += 1;
        }
        command_tail(std::slice::from_raw_parts(raw, len)).to_vec()
    }
}

pub fn is_compatibility(tail: &[u16]) -> bool {
    let end = tail
        .iter()
        .rposition(|&ch| ch != 32 && ch != 9)
        .map_or(0, |i| i + 1);
    ["--compatibility", "\"--compatibility\""]
        .iter()
        .any(|value| tail[..end].iter().copied().eq(value.encode_utf16()))
}
