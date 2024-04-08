use nalgebra::{Point3, Vector3};
use crate::{
    primitive::Primitive, 
    ray::Ray
};


pub struct Aabb {
    pub min: Point3<crate::Float>,
    pub max: Point3<crate::Float>,
}

impl Aabb {
    pub fn new(min: Point3<crate::Float>, max: Point3<crate::Float>) -> Self {
        Self { min, max }
    }
    
    pub fn intersect(&self, ray: &Ray) -> bool {
        let inv_dir = Vector3::new(1.0 / ray.direction.x, 1.0 / ray.direction.y, 1.0 / ray.direction.z);

        let t1 = (self.min - ray.origin) * &inv_dir;
        let t2 = (self.max - ray.origin) * &inv_dir;

        let t_min = t1.zip_map(&t2, |t1_i, t2_i| t1_i.min(t2_i));
        let t_max = t1.zip_map(&t2, |t1_i, t2_i| t1_i.max(t2_i));

        let t_enter = t_min.max();
        let t_exit = t_max.min();

        t_enter <= t_exit && t_exit > 0.0
    }

    pub fn from_primitives<T: Primitive>(primitives: &[T]) -> Self {
        let mut min = Point3::new(f64::INFINITY, f64::INFINITY, f64::INFINITY);
        let mut max = Point3::new(f64::NEG_INFINITY, f64::NEG_INFINITY, f64::NEG_INFINITY);

        for primitive in primitives {
            let bounding_box = primitive.bounding_box();
            min.x = min.x.min(bounding_box.min.x);
            min.y = min.y.min(bounding_box.min.y);
            min.z = min.z.min(bounding_box.min.z);
            max.x = max.x.max(bounding_box.max.x);
            max.y = max.y.max(bounding_box.max.y);
            max.z = max.z.max(bounding_box.max.z);
        }

        Self::new(min, max)
    }
}
