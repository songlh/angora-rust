use std::ptr;

fn main()  {
	let v: Vec<u8> = vec![1, 2, 3];
	let data = Some(v);

	let p = match data {
		Some(v) => v.as_ptr(),
		None => ptr::null(),
	};

	unsafe { println!("{:?}", *p); }
}