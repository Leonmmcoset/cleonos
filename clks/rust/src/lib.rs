#![no_std]

use core::hint::spin_loop;
use core::panic::PanicInfo;

extern "C" {
    fn clks_tty_write(text: *const i8);
}

const RUSTTEST_TEXT: &[u8] = b"Hello world!\n\0";

#[no_mangle]
pub extern "C" fn clks_rusttest_hello() {
    unsafe {
        clks_tty_write(RUSTTEST_TEXT.as_ptr() as *const i8);
    }
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {
        spin_loop();
    }
}

#[no_mangle]
pub extern "C" fn rust_eh_personality() {}