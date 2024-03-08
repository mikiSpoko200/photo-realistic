mod error;
mod raw;
mod export;

use std::{fs::File, ffi::CStr};
use std::io::BufReader;
pub use export::Vertex;

#[repr(C)]
pub struct Obj {
    pub n_vertices: usize,
    pub n_indices: usize,
    pub vertices: *const Vertex,
    pub indices: *const u16,
}

#[allow(non_snake_case)]
#[no_mangle]
pub unsafe extern "C" fn LoadOBJ(path: *const i8, obj: *mut Obj) {
    let cstr = unsafe { CStr::from_ptr(path) };
    let file = File::open(cstr.to_str().expect("path is valid utf-8")).expect(".obj file exists");
    let input = BufReader::new(file);
    let scene: export::Obj  = export::load_obj(input).expect("model loading is successful");
    let export::Obj { vertices, indices, .. } = scene;

    (*obj).n_vertices = vertices.len();
    (*obj).n_indices = indices.len();
    (*obj).vertices = vertices.as_ptr();
    (*obj).indices = indices.as_ptr();

    std::mem::forget(vertices);
    std::mem::forget(indices);
}

#[no_mangle]
pub unsafe extern "C" fn FreeOBJ(obj: Obj) {
    Vec::from_raw_parts(obj.vertices as *mut Vertex, obj.n_vertices, obj.n_vertices);
    Vec::from_raw_parts(obj.indices as *mut u16, obj.n_indices, obj.n_indices);
}
