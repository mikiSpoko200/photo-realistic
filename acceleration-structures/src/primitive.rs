use super::ray::{Ray, IntersectionInfo};
use crate::Aabb;


pub trait Primitive {
    fn intersect(&self, ray: &Ray, range: f32) -> Option<IntersectionInfo>;

    fn aabb(&self) -> Aabb;
}
