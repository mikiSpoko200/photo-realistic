use super::aabb::Aabb;
use super::ray::Ray;


// Types that can be tested for intersectino with a ray
pub trait Intersect {
    fn intersect(&self, ray: &Ray) -> Option<crate::Float>;
    fn bounding_box(&self) -> Aabb;
}
