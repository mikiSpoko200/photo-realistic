pub mod intersect;
pub mod primitive;
pub mod ray;
pub mod kd_tree;
pub mod aabb;

use aabb::Aabb;
use intersect::Intersect;
use nalgebra::{Point3, Vector3};

pub type Float = f32;
pub const N_DIMS: usize = 3;

// Example intersectable primitive (Sphere)
#[derive(Clone)]
struct Sphere {
    center: Point3<f64>,
    radius: f64,
}

impl Sphere {
    pub fn new(center: Point3<f64>, radius: f64) -> Self {
        Sphere { center, radius }
    }
}

impl Intersect for Sphere {
    fn intersect(&self, ray_origin: &ray::Ray) -> Option<f64> {
        let oc = *ray_origin - self.center;
        let a = ray_direction.norm_squared();
        let half_b = Vector3::dot(&oc, ray_direction);
        let c = oc.norm_squared() - self.radius * self.radius;

        let discriminant = half_b * half_b - a * c;
        if discriminant < 0.0 {
            None
        } else {
            let sqrt_d = discriminant.sqrt();
            let root = (-half_b - sqrt_d) / a;
            if root > 0.0 {
                Some(root)
            } else {
                let root = (-half_b + sqrt_d) / a;
                if root > 0.0 {
                    Some(root)
                } else {
                    None
                }
            }
        }
    }

    fn bounding_box(&self) -> Aabb {
        let r = Vector3::new(self.radius, self.radius, self.radius);
        Aabb::from_points(self.center - r, self.center + r)
    }
}

// Example usage
fn main() {
    let kd = todo!();
}
