use std::env;
use std::io;
use std::io::Read;
use std::fs::File;
use std::process;

fn main()  -> io::Result<()> {
	let mut args: Vec<String> = env::args().collect();

	if args.len() < 2 {
		return Ok(());
	}

	let f = args.remove(1);

	let fp = File::open(f)?;
	let mut buffer = [0; 255];
	
	let mut handle = fp.take(20);
	handle.read(&mut buffer)?;

	let x: u16;
	let y: i32;
	let z: i32;
	let a: u32;

	unsafe {
		x = *(&buffer[1] as * const u8 as * const u16);
	}

	unsafe {
		y = *(&buffer[4] as * const u8 as * const i32);
	}

	unsafe {
		z = *(&buffer[10] as * const u8 as * const i32);
	}

	unsafe {
		a = *(&buffer[14] as * const u8 as * const u32);
	}

	if x > 12300 && x < 12350 && z <  -100000000 && z > -100000005 && z != -100000003 && y >= 987654321 && y <= 987654325 && a == 123456789 {
		println!("hey, you hit it \n");
		process::abort();
	}

	return Ok(());
}