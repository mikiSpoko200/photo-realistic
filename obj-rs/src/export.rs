pub use crate::error::{LoadError, LoadErrorKind, ObjError, ObjResult};

use crate::raw;

use crate::error::make_error;
use crate::raw::object::Polygon;
use num_traits::FromPrimitive;
use std::collections::hash_map::{Entry, HashMap};
use std::io::BufRead;

/// Load a wavefront OBJ file into Rust & OpenGL friendly format.
pub fn load_obj<V: FromRawVertex<I>, T: BufRead, I>(input: T) -> ObjResult<Obj<V, I>> {
    let raw = crate::raw::parse_obj(input)?;
    Obj::new(raw)
}

/// 3D model object loaded from wavefront OBJ.
#[repr(C)]
#[derive(Debug)]
pub struct Obj<V = Vertex, I = u16> {
    /// Object's name.
    pub name: Option<String>,
    /// Vertex buffer.
    pub vertices: Vec<V>,
    /// Index buffer.
    pub indices: Vec<I>,
}

impl<V: FromRawVertex<I>, I> Obj<V, I> {
    /// Create `Obj` from `RawObj` object.
    pub fn new(raw: raw::RawObj) -> ObjResult<Self> {
        let (vertices, indices) =
            FromRawVertex::process(raw.positions, raw.normals, raw.tex_coords, raw.polygons)?;

        Ok(Obj {
            name: raw.name,
            vertices,
            indices,
        })
    }
}

/// Conversion from `RawObj`'s raw data.
pub trait FromRawVertex<I>: Sized {
    /// Build vertex and index buffer from raw object data.
    fn process(
        vertices: Vec<(f32, f32, f32, f32)>,
        normals: Vec<(f32, f32, f32)>,
        tex_coords: Vec<(f32, f32, f32)>,
        polygons: Vec<Polygon>,
    ) -> ObjResult<(Vec<Self>, Vec<I>)>;
}

/// Vertex data type of `Obj` which contains position and normal data of a vertex.
#[repr(C)]
#[derive(Default, Copy, PartialEq, Clone, Debug)]
pub struct Vertex {
    /// Position vector of a vertex.
    pub position: [f32; 3],
    /// Normal vector of a vertex.
    pub normal: [f32; 3],
}

impl<I: FromPrimitive + Copy> FromRawVertex<I> for Vertex {
    fn process(
        positions: Vec<(f32, f32, f32, f32)>,
        normals: Vec<(f32, f32, f32)>,
        _: Vec<(f32, f32, f32)>,
        polygons: Vec<Polygon>,
    ) -> ObjResult<(Vec<Self>, Vec<I>)> {
        let mut vb = Vec::with_capacity(polygons.len() * 3);
        let mut ib = Vec::with_capacity(polygons.len() * 3);
        {
            let mut cache = HashMap::new();
            let mut map = |pi: usize, ni: usize| -> ObjResult<()> {
                // Look up cache
                let index = match cache.entry((pi, ni)) {
                    // Cache miss -> make new, store it on cache
                    Entry::Vacant(entry) => {
                        let p = positions[pi];
                        let n = normals[ni];
                        let vertex = Vertex {
                            position: [p.0, p.1, p.2],
                            normal: [n.0, n.1, n.2],
                        };
                        let index = match I::from_usize(vb.len()) {
                            Some(val) => val,
                            None => make_error!(
                                IndexOutOfRange,
                                "Unable to convert the index from usize"
                            ),
                        };
                        vb.push(vertex);
                        entry.insert(index);
                        index
                    }
                    // Cache hit -> use it
                    Entry::Occupied(entry) => *entry.get(),
                };
                ib.push(index);
                Ok(())
            };

            for polygon in polygons {
                match polygon {
                    Polygon::P(_) | Polygon::PT(_) => make_error!(
                        InsufficientData,
                        "Tried to extract normal data which are not contained in the model"
                    ),
                    Polygon::PN(ref vec) if vec.len() == 3 => {
                        for &(pi, ni) in vec {
                            map(pi, ni)?;
                        }
                    }
                    Polygon::PTN(ref vec) if vec.len() == 3 => {
                        for &(pi, _, ni) in vec {
                            map(pi, ni)?;
                        }
                    }
                    _ => make_error!(
                        UntriangulatedModel,
                        "Model should be triangulated first to be loaded properly"
                    ),
                }
            }
        }
        vb.shrink_to_fit();
        Ok((vb, ib))
    }
}

/// Vertex data type of `Obj` which contains only position data of a vertex.
#[repr(C)]
#[derive(Default, Copy, PartialEq, Clone, Debug)]
pub struct Position {
    /// Position vector of a vertex.
    pub position: [f32; 3],
}

impl<I: FromPrimitive> FromRawVertex<I> for Position {
    fn process(
        vertices: Vec<(f32, f32, f32, f32)>,
        _: Vec<(f32, f32, f32)>,
        _: Vec<(f32, f32, f32)>,
        polygons: Vec<Polygon>,
    ) -> ObjResult<(Vec<Self>, Vec<I>)> {
        let vb = vertices
            .into_iter()
            .map(|v| Position {
                position: [v.0, v.1, v.2],
            })
            .collect();
        let mut ib = Vec::with_capacity(polygons.len() * 3);
        {
            let mut map = |pi: usize| -> ObjResult<()> {
                ib.push(match I::from_usize(pi) {
                    Some(val) => val,
                    None => make_error!(IndexOutOfRange, "Unable to convert the index from usize"),
                });
                Ok(())
            };

            for polygon in polygons {
                match polygon {
                    Polygon::P(ref vec) if vec.len() == 3 => {
                        for &pi in vec {
                            map(pi)?
                        }
                    }
                    Polygon::PT(ref vec) | Polygon::PN(ref vec) if vec.len() == 3 => {
                        for &(pi, _) in vec {
                            map(pi)?
                        }
                    }
                    Polygon::PTN(ref vec) if vec.len() == 3 => {
                        for &(pi, _, _) in vec {
                            map(pi)?
                        }
                    }
                    _ => make_error!(
                        UntriangulatedModel,
                        "Model should be triangulated first to be loaded properly"
                    ),
                }
            }
        }
        Ok((vb, ib))
    }
}

/// Vertex data type of `Obj` which contains position, normal and texture data of a vertex.
#[repr(C)]
#[derive(Default, Copy, PartialEq, Clone, Debug)]
pub struct TexturedVertex {
    /// Position vector of a vertex.
    pub position: [f32; 3],
    /// Normal vertor of a vertex.
    pub normal: [f32; 3],
    /// Texture of a vertex.
    pub texture: [f32; 3],
}

impl<I: FromPrimitive + Copy> FromRawVertex<I> for TexturedVertex {
    fn process(
        positions: Vec<(f32, f32, f32, f32)>,
        normals: Vec<(f32, f32, f32)>,
        tex_coords: Vec<(f32, f32, f32)>,
        polygons: Vec<Polygon>,
    ) -> ObjResult<(Vec<Self>, Vec<I>)> {
        let mut vb = Vec::with_capacity(polygons.len() * 3);
        let mut ib = Vec::with_capacity(polygons.len() * 3);
        {
            let mut cache = HashMap::new();
            let mut map = |pi: usize, ni: usize, ti: usize| -> ObjResult<()> {
                // Look up cache
                let index = match cache.entry((pi, ni, ti)) {
                    // Cache miss -> make new, store it on cache
                    Entry::Vacant(entry) => {
                        let p = positions[pi];
                        let n = normals[ni];
                        let t = tex_coords[ti];
                        let vertex = TexturedVertex {
                            position: [p.0, p.1, p.2],
                            normal: [n.0, n.1, n.2],
                            texture: [t.0, t.1, t.2],
                        };
                        let index = match I::from_usize(vb.len()) {
                            Some(val) => val,
                            None => make_error!(
                                IndexOutOfRange,
                                "Unable to convert the index from usize"
                            ),
                        };
                        vb.push(vertex);
                        entry.insert(index);
                        index
                    }
                    // Cache hit -> use it
                    Entry::Occupied(entry) => *entry.get(),
                };
                ib.push(index);
                Ok(())
            };

            for polygon in polygons {
                match polygon {
                    Polygon::P(_) => make_error!(InsufficientData, "Tried to extract normal and texture data which are not contained in the model"),
                    Polygon::PT(_) => make_error!(InsufficientData, "Tried to extract normal data which are not contained in the model"),
                    Polygon::PN(_) => make_error!(InsufficientData, "Tried to extract texture data which are not contained in the model"),
                    Polygon::PTN(ref vec) if vec.len() == 3 => {
                        for &(pi, ti, ni) in vec { map(pi, ni, ti)? }
                    }
                    _ => make_error!(UntriangulatedModel, "Model should be triangulated first to be loaded properly")
                }
            }
        }
        vb.shrink_to_fit();
        Ok((vb, ib))
    }
}
