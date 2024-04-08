use super::ray::{IntersectionInfo, Ray};
use super::primitive::Primitive;

use nalgebra_glm as glm;

const EPSILON: f32 = 0.000001;

pub struct Triangle {
    vertices: [glm::Vec3; 3]
}

impl Triangle {
    pub fn new(v0: glm::Vec3, v1: glm::Vec3, v2: glm::Vec3) -> Self {
        Self {
            vertices: [v0, v1, v2],
        }
    }
}

impl Primitive for Triangle {
    // TODO: test this
    fn intersect(&self, ray: &Ray, range: f32) -> Option<IntersectionInfo> {
        let [v0, v1, v2] = &self.vertices;
        let e1 = v1 - v0;
        let e2 = v2 - v0;

        let pvec = glm::cross(&ray.direction, &e2);
        let det = glm::dot(&e1, &pvec);

        if det.abs() < EPSILON {
            return None; // Ray is parallel to the triangle
        }

        let inv_det = 1.0 / det;

        let tvec = ray.origin - v0;
        let u = glm::dot(&tvec, &pvec) * inv_det;
        if u < 0.0 || u > 1.0 {
            return None;
        }

        let qvec = glm::cross(&tvec, &e1);
        let v = glm::dot(&ray.direction, &qvec) * inv_det;
        if v < 0.0 || u + v > 1.0 {
            return None;
        }

        let t = glm::dot(&e2, &qvec) * inv_det;

        Some(IntersectionInfo::new(glm::Vec3::new(t, u, v)))
    }

    fn aabb(&self) -> crate::Aabb {
        let [v0, v1, v2] = &self.vertices;
        let xs= {
            let min = [v0.x, v1.x, v2.x].into_iter().reduce(f32::min).unwrap();
            let max = [v0.x, v1.x, v2.x].into_iter().reduce(f32::max).unwrap();
            min..=max
        };
        let ys = {
            let min = [v0.y, v1.y, v2.y].into_iter().reduce(f32::min).unwrap();
            let max = [v0.y, v1.y, v2.y].into_iter().reduce(f32::max).unwrap();
            min..=max
        };
        let zs = {
            let min = [v0.z, v1.z, v2.z].into_iter().reduce(f32::min).unwrap();
            let max = [v0.z, v1.z, v2.z].into_iter().reduce(f32::max).unwrap();
            min..=max
        };
        crate::Aabb::new(xs, ys, zs)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn unit_triangle() -> Triangle {
        Triangle::new(
            glm::Vec3::new(-1.0, -1.0, 0.0),
            glm::Vec3::new(1.0, -1.0, 0.0),
            glm::Vec3::new(0.0, 1.0, 0.0),
        )
    }

    #[test]
    fn test_intersect_should_hit_1() {
        let triangle = unit_triangle();
        let ray = Ray::new(
            glm::Vec3::new(0.0, 0.0, -1.0),
            glm::Vec3::z()
        );

        let intersection = triangle.intersect(&ray, 10.0);
        assert!(matches!(intersection, Some(_)));
    }

    #[test]
    fn test_intersect_should_miss_1() {
        let triangle = unit_triangle();
        let ray = Ray::new(
            glm::Vec3::new(-1.0, 0.0, 0.0),
            glm::Vec3::x()
        );

        let intersection = triangle.intersect(&ray, 10.0);
        assert!(matches!(intersection, None));
    }

    #[test]
    fn test_intersect_should_miss_2() {
        let triangle = unit_triangle();
        let ray = Ray::new(
            glm::Vec3::new(1.0, 1.0, -1.0),
            glm::Vec3::z()
        );

        let intersection = triangle.intersect(&ray, 10.0);
        assert!(matches!(intersection, None));
    }
}