import math
import unittest

# CPU reference of the mapping installed in vr_xr.cpp / vr_stereo.cpp.
def eye_uv_into_symmetric_source(u, v, eye_fov, render_right, render_up):
    left, right, up, down = eye_fov
    sx = (math.tan(right) - math.tan(left)) / (2 * math.tan(render_right))
    bx = .5 + math.tan(left) / (2 * math.tan(render_right))
    sy = (math.tan(up) - math.tan(down)) / (2 * math.tan(render_up))
    by = .5 - math.tan(up) / (2 * math.tan(render_up))
    return u * sx + bx, v * sy + by

class StereoProjectionTests(unittest.TestCase):
    def test_symmetry_is_identity(self):
        for u,v in ((0,0),(.5,.5),(1,1),(.2,.82)):
            got = eye_uv_into_symmetric_source(u,v,(-.8,.8,.7,-.7),.8,.7)
            self.assertAlmostEqual(got[0],u,places=6)
            self.assertAlmostEqual(got[1],v,places=6)

    def test_runtime_eye_frusta_map_corners_to_expected_rays(self):
        # Representative asymmetric left eye: wider to the nose than toward the temple.
        eye=(-.84,.74,.79,-.69)
        render_r, render_u=1.05,.99
        for u,v in ((0,0),(.5,.5),(1,1),(.2,.9)):
            src=eye_uv_into_symmetric_source(u,v,eye,render_r,render_u)
            ray_x=(src[0]-.5)*2*math.tan(render_r)
            ray_y=(.5-src[1])*2*math.tan(render_u)
            eye_ray_x=math.tan(eye[0])*(1-u)+math.tan(eye[1])*u
            eye_ray_y=math.tan(eye[2])*(1-v)+math.tan(eye[3])*v
            self.assertAlmostEqual(ray_x,eye_ray_x,places=5)
            self.assertAlmostEqual(ray_y,eye_ray_y,places=5)

    def test_per_eye_principal_points_can_differ(self):
        left=eye_uv_into_symmetric_source(.5,.5,(-.89,.68,.76,-.74),1.02,.98)
        right=eye_uv_into_symmetric_source(.5,.5,(-.68,.89,.76,-.74),1.02,.98)
        self.assertNotAlmostEqual(left[0],right[0],places=2)
        self.assertAlmostEqual(left[1],right[1],places=6)

    def test_wide_render_frustum_keeps_ray_corners_in_frame(self):
        fov=(-.84,.74,.79,-.69)
        for uv in ((0,0),(0,1),(1,0),(1,1)):
            p=eye_uv_into_symmetric_source(*uv,fov,1.0,.99)
            self.assertTrue(0<=p[0]<=1)
            self.assertTrue(0<=p[1]<=1)

if __name__=="__main__":
    unittest.main()
