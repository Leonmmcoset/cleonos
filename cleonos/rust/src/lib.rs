#![no_std]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {
        core::hint::spin_loop();
    }
}

#[no_mangle]
pub extern "C" fn cleonos_rust_guarded_len(ptr: *const u8, max_len: usize) -> u64 {
    let mut i: usize = 0;

    if ptr.is_null() {
        return 0;
    }

    while i < max_len {
        let ch = unsafe { *ptr.add(i) };

        if ch == 0 {
            break;
        }

        i += 1;
    }

    i as u64
}
