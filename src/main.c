#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include "func.h"
#include "display.h"
#include "vector.h"
#include "mesh.h"
#include "array.h"
#include "matrix.h"
#include "misc.h" // string stuff
#include "light.h"
#include "texture.h"
#include "draw_triangle_pikuma.h"
#include "draw_triangle_torb.h"
#include "stretchy_buffer.h"
#include "camera.h"
#include "clip.h"
#include "render_font/software_bitmapfont.h"


// The going could get rough

enum eCull_method {
    CULL_NONE,
    CULL_BACKFACE
};

enum eRender_method {
    RENDER_WIRE,
    RENDER_WIRE_VERTEX,
    RENDER_FILL_TRIANGLE,
    RENDER_FILL_TRIANGLE_WIRE,
    RENDER_TEXTURED,
    RENDER_TEXTURED_WIRE
};

enum eCull_method cull_method;
enum eRender_method render_method;

// Review of structs

mat4_t normal_matrix;


struct uniforms_t
{
    mat4_t model_matrix;
    mat4_t view_matrix;
    mat4_t projection_matrix;
} uniforms;

typedef struct {
    vec4_t a,b;
} line_t;

bool sort_faces_by_z_enable = true;
bool display_normals_enable = false;
bool draw_triangles_torb = true;

//#define DYNAMIC_MEM_EACH_FRAME 1

triangle_t *triangles_to_render = NULL;
line_t* lines_to_render = NULL;

bool is_running = false;
unsigned int previous_frame_time = 0;
int num_culled;
int num_cull_backface;
int num_cull_zero_area;
int num_cull_small_area;
int num_cull_degenerate;
int num_not_culled;
int num_cull_near;
int num_cull_far;
int num_cull_xy;
int num_cull_few;
int num_cull_many;

int vertex_time_start = 0;
int vertex_time_end = 0;

typedef struct {
    int x,y,z;
    float dx;
    float dy;
    float ox;
    float oy;
    bool left,right,middle;
} mouse_t;
mouse_t mouse;

float z_near = 0.01f;
float z_far = 200.0f;

void setup(const char* mesh_file, const char* texture_file) {
    //stb__sbgrow(triangles_to_render,1024*8);

    memset(&mouse,0,sizeof(mouse));

    // Initialize render mode and triangle culling method
    render_method = RENDER_WIRE;
    cull_method = CULL_BACKFACE;

    float aspect_y = (float)get_window_height() / (float)get_window_width();
    float aspect_x = (float)get_window_width() / (float)get_window_height();
    float fov_y = PI / 3.0; // the same as 180/3, or 60deg
    float fov_x = atan(tan(fov_y / 2.f) * aspect_x) * 2.f;


    uniforms.projection_matrix = mat4_make_perspective(fov_y, aspect_y, z_near, z_far);
    camera.position = (vec3_t){0,0,0};
    init_frustum_planes(fov_x, fov_y, z_near, z_far);//, frustum_planes);

    //load_cube_mesh_data();
    load_obj_file_data(mesh_file);
    load_png_texture_data(texture_file);
}

uint32_t vec3_to_uint32_t(vec3_t c)
{
    return packColor( (U8)c.x*255, (U8)c.y*255, (U8)c.z*255);
}

void process_input(void) {
    SDL_Event event;
    while ( SDL_PollEvent(&event) )
    {

        switch(event.type) {
        case SDL_QUIT:
            SDL_Log("SDL_QUIT\n");
            is_running = false;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
                is_running = false;
            if (event.key.keysym.sym == SDLK_1)
                render_method = RENDER_WIRE_VERTEX;
            if (event.key.keysym.sym == SDLK_2)
                render_method = RENDER_WIRE;
            if (event.key.keysym.sym == SDLK_3)
                render_method = RENDER_FILL_TRIANGLE;
            if (event.key.keysym.sym == SDLK_4)
                render_method = RENDER_FILL_TRIANGLE_WIRE;
            if (event.key.keysym.sym == SDLK_5)
                render_method = RENDER_TEXTURED;
            if (event.key.keysym.sym == SDLK_6)
                render_method = RENDER_TEXTURED_WIRE;


            if (event.key.keysym.sym == SDLK_UP)
                mesh.translation.y += 0.5f;
            if (event.key.keysym.sym == SDLK_DOWN)
                mesh.translation.y -= 0.5f;
            if (event.key.keysym.sym == SDLK_LEFT)
                mesh.translation.x += 0.5f;
            if (event.key.keysym.sym == SDLK_RIGHT)
                mesh.translation.x -= 0.5f;

            if (event.key.keysym.sym == SDLK_PAGEUP)
                camera.position.z += 0.5f;
            if (event.key.keysym.sym == SDLK_PAGEDOWN)
                camera.position.z -= 0.5f;




            if (event.key.keysym.sym == SDLK_c)
            {
                cull_method = CULL_BACKFACE;
                SDL_Log("cull back\n");
            }
            if (event.key.keysym.sym == SDLK_d)
            {
                cull_method = CULL_NONE;
                SDL_Log("cull off\n");
            }
            if (event.key.keysym.sym == SDLK_z)
            {
                sort_faces_by_z_enable = true;
                SDL_Log("sort by z ON\n");
            }
            if (event.key.keysym.sym == SDLK_x)
            {
                sort_faces_by_z_enable = false;
                SDL_Log("sort by z off\n");
            }
            if (event.key.keysym.sym == SDLK_n)
                display_normals_enable = true;

            if (event.key.keysym.sym == SDLK_t)
            {
                SDL_Log("enable torb\n");
                draw_triangles_torb = true;
            }
            if (event.key.keysym.sym == SDLK_r)
            {
                SDL_Log("disable torb\n");
                draw_triangles_torb = false;
            }
            break;

        case SDL_KEYUP:
        {
            if (event.key.keysym.sym == SDLK_n)
                display_normals_enable = false;
            break;
        }
        case SDL_MOUSEMOTION:
        {
            mouse.x = event.motion.x;
            mouse.y = event.motion.y;

            if (event.motion.state & SDL_BUTTON_LMASK)
            {
                mouse.left = 1;
            }
            if (event.motion.state & SDL_BUTTON_RMASK)
            {
                mouse.right = 1;
            }
            if (event.motion.state & SDL_BUTTON_MMASK)
            {
                mouse.middle = 1;
            }
        }
        break;
        case SDL_MOUSEBUTTONDOWN:
        {
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                mouse.left = 1;
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
            {
                mouse.right = 1;
            }
            if (event.button.button & SDL_BUTTON_MMASK)
            {
                mouse.middle = 1;
            }
        }
        break;
        case SDL_MOUSEBUTTONUP:
        {
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                mouse.left = 0;
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
            {
                mouse.right = 0;
            }
            if (event.button.button & SDL_BUTTON_MMASK)
            {
                mouse.middle = 0;
            }
        }
        break;

        case SDL_MOUSEWHEEL:
        {
            if(event.wheel.y > 0) // scroll up
            {
                mouse.z--;
            }
            else if(event.wheel.y < 0) // scroll down
            {
                // Put code for handling "scroll down" here!
                mouse.z++;
            }
        }
        break;
        }
    }// while


    mouse.dx = mouse.x - mouse.ox;
    mouse.dy = mouse.y - mouse.oy;
    mouse.ox = mouse.x;
    mouse.oy = mouse.y;
}

static void snap(float *v)
{
  *v = floorf( (*v) * 128.f)/128.f;
}

vec4_t to_screen_space(vec4_t v)
{
    v.x *= (get_window_width() / 2.0f);
    v.y *= (get_window_height() / 2.0f);

    // Invert screen y coordinate since our display goes from 0 to get_window_height()
    v.y *= -1;

    v.x += (get_window_width() / 2.0f);
    v.y += (get_window_height() / 2.0f);

    snap(&v.x);
    snap(&v.y);

    return v;
}

static bool isBackface(vec2_t a, vec2_t b, vec2_t c, float *area2)
{
    vec2_t vec1 = vec2_sub(b,c);
    vec2_t vec2 = vec2_sub(a,c);
    *area2 = fabsf((vec1.x * vec2.y) - (vec1.y * vec2.x));
    if (vec2.x*vec1.y >= vec2.y*vec1.x) return 1; // change to < for right hand coords or >= for left
    return 0;
}


static void addLineToRender(vec3_t normal, vec3_t center, mat4_t mvp_matrix)
{
  float line_length = 20.f / (.5f*get_window_width());
  normal = vec3_mul(normal, line_length);
  vec4_t start = mat4_mul_vec4_project(mvp_matrix, vec4_from_vec3(center) );
  vec4_t end = mat4_mul_vec4_project(mvp_matrix, vec4_from_vec3( vec3_add(center, normal)) );
  if (start.w < 0 && end.w < 0) return; // TODO proper 3D line clipping
  line_t projected_line = {.a = to_screen_space(start), .b = to_screen_space(end) };
  array_push(lines_to_render, projected_line);
}

static void addTriangleToRender(triangle_t projected_triangle)
{
  // Save the projected triangle in the array of triangles to render
#if defined(DYNAMIC_MEM_EACH_FRAME)
    array_push(triangles_to_render, projected_triangle);
#else
  sb_push(triangles_to_render, projected_triangle);
#endif
}

static float smoothstep_inv(float A, float B, float v)
{
    //float v = i / N;
    v = 1.0f - (1.0f - v) * (1.0f - v);
    return (A * v) + (B * (1.0f - v));
}

void update(void) {
    // Wait some time until we reach the target frame time
    Uint32 sdl_time = SDL_GetTicks();
    int time_to_wait =
        (int)(previous_frame_time + FRAME_TARGET_TIME) - sdl_time;

    time_to_wait = FRAME_TARGET_TIME - (sdl_time - (int)previous_frame_time);

    // Only delay if we are too fast
    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) SDL_Delay(time_to_wait);
    previous_frame_time = SDL_GetTicks();

    // Initialize the array of triangles to render
#if defined(DYNAMIC_MEM_EACH_FRAME)
    triangles_to_render = NULL;
#else
    // Set Stretchy Buffer size (not capacity) to zero
    if (triangles_to_render != NULL) stb__sbn(triangles_to_render)=0;
#endif

    if (mouse.right) mesh.rotation.x += .01f*mouse.dx;
    if (mouse.right) mesh.rotation.y += .01f*mouse.dy;
    //mesh.rotation.z = 0;

    //mesh.rotation.x = .25*time;
    //mesh.rotation.z = .5*time;
    //mesh.rotation.y = -M_PI;
    //mesh.rotation.z = 0;

    //mesh.translation.x = 0;
    static float time = 0.0f;
    static float dir = 1.0f;
    //mesh.translation.y = smoothstep_inv(-1, 1, fmod(time, 1.0f) );
    //mesh.translation.y = smoothstep_inv(1, -1, time );

    if (time > 1.0)
    {
        time = 1;
        dir *= -1;
    }

    if (time < 0.0)
    {
        time = 0;
        dir *= -1;
    }
    time += dir * 0.02;
    mesh.translation.z = 3.5f + mouse.z;
    mesh.scale.y = 1.0 + smoothstep_inv(0.0, -0.6, 0.2 + .8*time );


    // Initialize the target looking at the positive z-axis
    vec3_t target = { 0, 0, 1 };
    mat4_t camera_yaw_rotation = mat4_make_rotation_y(0);//camera.yaw);
    camera.direction = vec3_from_vec4(mat4_mul_vec4(camera_yaw_rotation, vec4_from_vec3(target)));

    // Offset the camera position in the direction where the camera is pointing at
    target = vec3_add(camera.position, camera.direction);
    vec3_t up_direction = { 0, 1, 0 };

    // Create the view matrix
    uniforms.view_matrix = mat4_look_at(camera.position, target, up_direction);

    // Create scale, rotation, and translation matrices that will be used to multiply the mesh vertices
    mat4_t scale_matrix = mat4_make_scale(mesh.scale.x, mesh.scale.y, mesh.scale.z);
    mat4_t translation_matrix = mat4_make_translation(mesh.translation.x, mesh.translation.y, mesh.translation.z);
    mat4_t rotation_matrix_x = mat4_make_rotation_x(mesh.rotation.x);
    mat4_t rotation_matrix_y = mat4_make_rotation_y(mesh.rotation.y);
    mat4_t rotation_matrix_z = mat4_make_rotation_z(mesh.rotation.z);

    // Create a World Matrix combining scale, rotation, and translation matrices
    mat4_t world_matrix = mat4_identity();

    // Order matters: First scale, then rotate, then translate. [T]*[R]*[S]*v
    world_matrix = mat4_mul_mat4(scale_matrix, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_z, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_y, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_x, world_matrix);
    world_matrix = mat4_mul_mat4(translation_matrix, world_matrix);


    normal_matrix = world_matrix;
    uniforms.model_matrix = world_matrix;


    light.position.x = 0;
    light.position.y = 0;
    light.position.z = 0;
    light.position_proj = vec4_from_vec3(light.position);

    mat4_t light_world_matrix = mat4_identity();

    //mat4_t light_rotation_matrix_y = mat4_make_rotation_y(.5f*(float)time);
    //translation_matrix = mat4_make_translation(0,0,-6);
    //light_world_matrix = mat4_mul_mat4(translation_matrix, light_world_matrix);
    //light_world_matrix = mat4_mul_mat4(light_rotation_matrix_y, light_world_matrix);
    translation_matrix = mat4_make_translation(0,0,-5);
    light_world_matrix = mat4_mul_mat4(translation_matrix, light_world_matrix);

    light.position = vec3_from_vec4( mat4_mul_vec4(light_world_matrix, light.position_proj) );

    const mat4_t light_mvp = mat4_mul_mat4(uniforms.projection_matrix, light_world_matrix);
    vec4_t light_transformed = mat4_mul_vec4_project(light_mvp, light.position_proj);
    light.position_proj = to_screen_space(light_transformed);
}

void vertexShading(mat4_t model_matrix, mat4_t view_matrix, mat4_t projection_matrix)
{
    // mvp = M V P

    mat4_t mvp = mat4_mul_mat4(projection_matrix, view_matrix);
    mvp = mat4_mul_mat4(mvp,model_matrix);

    vertex_time_start = SDL_GetTicks();
    num_culled = 0;
    num_cull_backface = 0;
    num_cull_zero_area = 0;
    num_cull_small_area = 0;
    num_cull_degenerate = 0;
    num_cull_near = 0;
    num_cull_far = 0;
    num_not_culled = 0;
    num_cull_xy = 0;
    num_cull_few = 0;
    num_cull_many = 0;
    lines_to_render = NULL;

    // Loop all triangle faces of our mesh
    // It would be more efficient to extract all verts, texcoords, normals from mesh on startup to a mesh VertexBuffer
    // and then transform all vertices in a tight loop
    // and then draw them using an index buffer
    int n_faces = array_length(mesh.faces);
    for (int i = 0; i < n_faces; i++) {
        face_t mesh_face = mesh.faces[i];

        vec3_t face_vertices[3];
        face_vertices[0] = mesh.vertices[mesh_face.a];
        face_vertices[1] = mesh.vertices[mesh_face.b];
        face_vertices[2] = mesh.vertices[mesh_face.c];

        vec2_t face_texcoords[3];
        face_texcoords[0] = mesh.texcoords[mesh_face.texcoord_a];
        face_texcoords[1] = mesh.texcoords[mesh_face.texcoord_b];
        face_texcoords[2] = mesh.texcoords[mesh_face.texcoord_c];

        uint32_t face_colors[3];
        face_colors[0] = 0xFFFF0000;//vec3_to_uint32_t(mesh.colors[mesh_face.a ]);
        face_colors[1] = 0xFF00FF00;//vec3_to_uint32_t(mesh.colors[mesh_face.b ]);
        face_colors[2] = 0xFF0000FF;//vec3_to_uint32_t(mesh.colors[mesh_face.c ]);

        vec3_t center = {0,0,0};
        center = vec3_add(center, face_vertices[0]);
        center = vec3_add(center, face_vertices[1]);
        center = vec3_add(center, face_vertices[2]);
        center = vec3_mul(center, 1.0f / 3.0f);


        vec4_t transformed_vertices[3];
        // Loop all three vertices of this current face and apply transformations
        for (int j = 0; j < 3; j++) {
            // Transform face by concatenated MVP matrix, then divide by W (projection)
            //vec4_t transformed_vertex = mat4_mul_vec4_project(mvp, vec4_from_vec3(face_vertices[j]) );
            // Multiply the world matrix by the original vector
            vec4_t transformed_vertex = mat4_mul_vec4(model_matrix,  vec4_from_vec3(face_vertices[j]));

            // Multiply the view matrix by the vector to transform the scene to camera space
            transformed_vertex = mat4_mul_vec4(view_matrix, transformed_vertex);

            // Save transformed vertex in the array of transformed vertices
            transformed_vertices[j] = transformed_vertex;
        }

        // Save world space triangle
        polygon_t polygon = create_polygon_from_triangle(
            vec3_from_vec4(transformed_vertices[0]),
            vec3_from_vec4(transformed_vertices[1]),
            vec3_from_vec4(transformed_vertices[2]),
            face_texcoords[0],
            face_texcoords[1],
            face_texcoords[2]
        );

        int num_zero_w = 0;
        int num_out_near = 0;
        int num_out_far = 0;

        for (int j = 0; j < 3; j++) {
            num_zero_w += transformed_vertices[j].w <= 0.f;
            num_out_near += transformed_vertices[j].z < z_near;
            num_out_far += transformed_vertices[j].z > z_far;
        }

        if (num_zero_w == 3)
        {
            num_cull_zero_area++;
            num_culled++;
            continue;
        }

        if (num_out_near == 3)
        {
            num_cull_near++;
            num_culled++;
            continue;
        }

        if (num_out_far == 3)
        {
            num_cull_far++;
            num_culled++;
            continue;
        }

        // Before transforming to screen space, find area in world space
        // Check backface culling
        vec3_t vector_a = vec3_from_vec4(transformed_vertices[0]); /*   A   */
        vec3_t vector_b = vec3_from_vec4(transformed_vertices[1]); /*  / \  */
        vec3_t vector_c = vec3_from_vec4(transformed_vertices[2]); /* C---B */

        // Get the vector subtraction of B-A and C-A
        vec3_t vector_ab = vec3_sub(vector_b, vector_a);
        vec3_t vector_ac = vec3_sub(vector_c, vector_a);
        vec3_t transformed_normal = vec3_cross(vector_ab, vector_ac);

        // Scale and translate the projected points to the middle of the screen
        for (int j = 0; j < 3; j++) {
            transformed_vertices[j] = mat4_mul_vec4_project( projection_matrix, transformed_vertices[j] );
            transformed_vertices[j] = to_screen_space(transformed_vertices[j]);
        }



        vec3_t face_normal = vec3_cross(
                            vec3_sub(face_vertices[1],face_vertices[0]),
                            vec3_sub(face_vertices[2],face_vertices[0])
                        );
        vec3_normalize(&face_normal);

        // Find the vector between a point in the triangle and camera origin
        vec3_t origin = {0.f, 0.f, 0.f};
        vec3_t camera_ray = vec3_sub(origin, vector_a);
        float rayDotNormal = vec3_dot(camera_ray, transformed_normal);

        // Bypass the triangles looking away from camera
        float area2 = 0;
        bool backfacing = isBackface(
              (vec2_t) {transformed_vertices[0].x, transformed_vertices[0].y},
              (vec2_t) {transformed_vertices[1].x, transformed_vertices[1].y},
              (vec2_t) {transformed_vertices[2].x, transformed_vertices[2].y},
              &area2
                          );
        //float area2 = vec3_dot(transformed_normal, transformed_normal);
        if (cull_method == CULL_BACKFACE && !backfacing)
        {
            num_cull_backface++;
            num_culled++;
            continue;
        }
//        if ( area2 < 0.000001f)
//        {
//            // Think of a triangle that is 10 wide, 10 high. it has an area of 100 / 2 = 50
//            // Triangles can be surprisingly small yet contribute to image
//            num_cull_small_area++;
//            num_culled++;
//            continue;
//        }

//        bool front_facing = rayDotNormal > 0.0f;
//        //front_facing = !backfacing;
//        assert(backfacing == !front_facing);
//        if (cull_method == CULL_BACKFACE && !front_facing)
//        {
//            num_cull_backface++;
//            num_culled++;
//            continue;
//        }


          clip_polygon(&polygon); //, frustum_planes);
          if (polygon.num_vertices < 3)
          {
              num_cull_few++;
              num_culled++;
              continue;
          }

          if (polygon.num_vertices==3)
          {
              if (display_normals_enable)
              {
                  addLineToRender(face_normal, center, mvp);
              }
          }

          vec4_t transformed_and_clipped_vertices[3];
          /*
          if (polygon.num_vertices == 3)
          {
              int index0 = 0;
              int index1 = 1;
              int index2 = 2;
              triangle_t projected_triangle;
              projected_triangle.z = 0.f;


              transformed_and_clipped_vertices[0] = vec4_from_vec3(polygon.vertices[index0]); // Notice always first
              transformed_and_clipped_vertices[1] = vec4_from_vec3(polygon.vertices[index1]);
              transformed_and_clipped_vertices[2] = vec4_from_vec3(polygon.vertices[index2]);
              for (int j=0; j<3; j++)
              {
                vec4_t projected_point = mat4_mul_vec4_project( projection_matrix, transformed_and_clipped_vertices[j] );
                projected_triangle.points[j] = to_screen_space(projected_point);
              }


              vec2_t a = vec2_sub(
                        vec2_from_vec4( projected_triangle.points[1] ),
                        vec2_from_vec4( projected_triangle.points[2] ) );
              vec2_t b = vec2_sub(
                        vec2_from_vec4( projected_triangle.points[0] ),
                        vec2_from_vec4( projected_triangle.points[2] ) );

              float area2 = fabsf((a.x * b.y) - (a.y * b.x));
              projected_triangle.area2 = area2;

              projected_triangle.texcoords[0] = polygon.texcoords[index0];
              projected_triangle.texcoords[1] = polygon.texcoords[index1];
              projected_triangle.texcoords[2] = polygon.texcoords[index2];
              addTriangleToRender(projected_triangle);
          }
          else*/
          {
            for(int tri=0; tri<polygon.num_vertices - 2; tri++)
            {
              transformed_and_clipped_vertices[0] = vec4_from_vec3(polygon.vertices[0]); // Notice always first
              transformed_and_clipped_vertices[1] = vec4_from_vec3(polygon.vertices[tri+1]);
              transformed_and_clipped_vertices[2] = vec4_from_vec3(polygon.vertices[tri+2]);

              triangle_t projected_triangle;
              projected_triangle.z = 0.f;
              for (int j=0; j<3; j++)
              {
                vec4_t projected_point = mat4_mul_vec4_project( projection_matrix, transformed_and_clipped_vertices[j] );

                projected_point = to_screen_space(projected_point);
                // TODO add culling if all 2 xes or 2 ys same

                projected_triangle.points[j].x = projected_point.x;
                projected_triangle.points[j].y = projected_point.y;
                projected_triangle.points[j].z = projected_point.z;
                projected_triangle.points[j].w = projected_point.w;

                projected_triangle.z = fmax( projected_triangle.z, projected_point.z );

              }

              bool two_points_same =
                      (projected_triangle.points[0].x == projected_triangle.points[1].x && projected_triangle.points[0].y == projected_triangle.points[1].y) ||
                      (projected_triangle.points[1].x == projected_triangle.points[2].x && projected_triangle.points[1].y == projected_triangle.points[2].y) ||
                      (projected_triangle.points[0].x == projected_triangle.points[2].x && projected_triangle.points[0].y == projected_triangle.points[2].y);
              if (two_points_same)
              {
                  num_cull_degenerate++;
                  //num_culled++; // Since we clipped, we may have more tris
                  continue;
              }


              projected_triangle.texcoords[0] = polygon.texcoords[0];
              projected_triangle.texcoords[1] = polygon.texcoords[tri+1];
              projected_triangle.texcoords[2] = polygon.texcoords[tri+2];

              vec2_t vec1 = vec2_sub(
                        vec2_from_vec4( projected_triangle.points[1] ),
                        vec2_from_vec4( projected_triangle.points[2] ) );
              vec2_t vec2 = vec2_sub(
                        vec2_from_vec4( projected_triangle.points[0] ),
                        vec2_from_vec4( projected_triangle.points[2] ) );

              float area2 = fabsf((vec1.x * vec2.y) - (vec1.y * vec2.x));
              projected_triangle.area2 = area2;

              projected_triangle.center = center;
              projected_triangle.normal = face_normal;
              // projected_triangle.colors = face_colors[0]; // TODO lerp colors

              addTriangleToRender(projected_triangle);
            }
          }
    }
    vertex_time_end = SDL_GetTicks();
}

int cmpLess(const void *triangleA, const void *triangleB) {
    triangle_t a=*((triangle_t*)triangleA);
    triangle_t b=*((triangle_t*)triangleB);
    if(a.z > b.z)
        return -1;
    else if(a.z < b.z)
        return 1;
    return 0;
}

void sort(void)
{
#if defined(DYNAMIC_MEM_EACH_FRAME)
    int num_tris = array_length(triangles_to_render);
#else
    int num_tris = sb_count(triangles_to_render);
#endif
    qsort(triangles_to_render, num_tris, sizeof(triangle_t), cmpLess);
}

void draw_list_of_triangles(int option)
{
#if defined(DYNAMIC_MEM_EACH_FRAME)
    int num_tris = array_length(triangles_to_render);
#else
    int num_tris = sb_count(triangles_to_render);
#endif
    uint32_t color = packColor(255,255,255);
    //if (!mouse.left)
    for (int i = 0; i < num_tris; i++) {
        triangle_t triangle = triangles_to_render[i];

        // Draw filled triangle
        if (render_method == RENDER_FILL_TRIANGLE || render_method == RENDER_FILL_TRIANGLE_WIRE) {

            //float z = triangle.z <= 0.f ? 0.01f : triangle.z;
            //float c = 255-255/5.f*z;
            //c = c > 255 ? 255.f : c;
            //c = c < 0 ? 0.0f : c;
            //triangle.colors
            light.direction = vec3_sub(light.position, triangle.center );
            vec3_normalize(&light.direction);

            vec4_t normal = vec4_from_vec3(triangle.normal);
            normal.w = 0;
            vec3_t normalInterp = vec3_from_vec4(mat4_mul_vec4(normal_matrix, normal ));
            vec3_normalize(&normalInterp);
            float n_dot_l = vec3_dot(normalInterp, light.direction);
            if (n_dot_l < 0.0f) n_dot_l = 0.0f;
            uint32_t color_lit = light_apply_intensity(color, n_dot_l);
            uint32_t colors[3] = {color_lit,color_lit,color_lit};

            draw_triangle(
                triangle.points[0].x, triangle.points[0].y, triangle.points[0].z, triangle.points[0].w,// vertex A
                triangle.points[1].x, triangle.points[1].y, triangle.points[1].z, triangle.points[1].w, // vertex B
                triangle.points[2].x, triangle.points[2].y, triangle.points[2].z, triangle.points[2].w, // vertex C
                colors
            );
        }

        // Draw filled triangle
        if (render_method == RENDER_TEXTURED || render_method == RENDER_TEXTURED_WIRE) {

            //float z = triangle.z <= 0.f ? 0.01f : triangle.z;
            //float c = 255-255/5.f*z;
            //c = c > 255 ? 255.f : c;
            //c = c < 0 ? 0.0f : c;
            //triangle.colors
            light.direction = vec3_sub(light.position, triangle.center );
            vec3_normalize(&light.direction);

            vec4_t normal = vec4_from_vec3(triangle.normal);
            normal.w = 0;
            vec3_t normalInterp = vec3_from_vec4(mat4_mul_vec4(normal_matrix, normal ));
            vec3_normalize(&normalInterp);
            float n_dot_l = vec3_dot(normalInterp, light.direction);
            if (n_dot_l < 0.0f) n_dot_l = 0.0f;
            uint32_t color_lit = light_apply_intensity(color, n_dot_l);
            uint32_t colors[3] = {color_lit,color_lit,color_lit};

            vertex_texcoord_t vertices[3];
            for(int vtx=0; vtx<3; vtx++)
            {
                vertices[vtx].x = triangle.points[vtx].x;
                vertices[vtx].y = triangle.points[vtx].y;
                vertices[vtx].z = triangle.points[vtx].z;
                vertices[vtx].w = triangle.points[vtx].w;
                vertices[vtx].u = triangle.texcoords[vtx].x;
                vertices[vtx].v = triangle.texcoords[vtx].y;
            }

            if (option==0)
            {
                //texture_t texture = {.texels = (uint8_t*)&REDBRICK_TEXTURE[0], .width=64, .height=64};
                texture_t texture = {.texels = (uint8_t*)&mesh_texture[0], .width=texture_width, .height=texture_height};
                draw_triangle_textured(vertices[0], vertices[1], vertices[2], &texture, colors, triangle.area2);
            }
            else
            {
                //texture_t texture = {.texels = (uint8_t*)&mesh_texture[0], .width=texture_width, .height=texture_height};
                draw_triangle_textured_p(vertices[0], vertices[1], vertices[2], mesh_texture);
            }
        }


        // Draw triangle wireframe
        if (render_method == RENDER_WIRE ||
                render_method == RENDER_WIRE_VERTEX ||
                render_method == RENDER_FILL_TRIANGLE_WIRE ||
                render_method == RENDER_TEXTURED_WIRE
           ) {
            draw_triangle_lines(
                triangle.points[0].x, triangle.points[0].y, // vertex A
                triangle.points[1].x, triangle.points[1].y, // vertex B
                triangle.points[2].x, triangle.points[2].y, // vertex C
                0xFF000000
            );
        }

        // Draw triangle vertex points
        if (render_method == RENDER_WIRE_VERTEX) {
            draw_rect(triangle.points[0].x - 3, triangle.points[0].y - 3, 6, 6, 0xFFFF0000); // vertex A
            draw_rect(triangle.points[1].x - 3, triangle.points[1].y - 3, 6, 6, 0xFFFF0000); // vertex B
            draw_rect(triangle.points[2].x - 3, triangle.points[2].y - 3, 6, 6, 0xFFFF0000); // vertex C
        }
    }
}

void render(void) {
    clear_color_buffer( packColor(255,0,255) );
    clear_z_buffer( 1.0f );
    draw_grid();

    if (sort_faces_by_z_enable) sort();

    // Loop all projected triangles and render them
    uint32_t color = 0xFFFFFFFF;
    if (light.position_proj.w > 0.1f)
        circle(light.position_proj.x, light.position_proj.y, 20 - light.position_proj.z);
    circle(mouse.x, mouse.y, 10 + draw_triangles_torb*10.f);

    int ms = SDL_GetTicks();
    if (draw_triangles_torb) draw_list_of_triangles(0);
    int ms1 = SDL_GetTicks();
    if (!draw_triangles_torb) draw_list_of_triangles(1);
    int ms2 = SDL_GetTicks();

    int vertex_time = vertex_time_end - vertex_time_start;
#if defined(DYNAMIC_MEM_EACH_FRAME)
    int num_triangles_to_render = array_length(triangles_to_render);
#else
    int num_triangles_to_render = sb_count(triangles_to_render);
#endif

    static int numframes=0;
    numframes++;
    int time1 = ms1 - ms;
    int time2 = ms2 - ms;
    float timediff = 0.f;
    if (time2>0 && time1>0) timediff = time1/(float)time2;

    static int frames_per_second = 0;
    static int old_time = 0;
    static int old_frames = 0;
    int time = SDL_GetTicks();
    if (old_time < time)
    {
      old_time = time + 1000;
      frames_per_second = numframes - old_frames;
      old_frames = numframes;

    }



    if (display_normals_enable)
    {
        color = 0xFF00FF00;
        int num_lines = array_length(lines_to_render);
        for (int i = 0; i < num_lines; i++) {
            line_t line = lines_to_render[i];
            draw_line3d(line.a.x, line.a.y, line.a.w, line.b.x, line.b.y, line.b.w, color);
        }

    }

    //draw_triangle(300, 100, 50, 400, 500, 700, 0xFF00FF00);
    draw_triangle_textured_p(
    (vertex_texcoord_t){ 0,   get_window_height()-0, 0.0f,   1.0f, 0.0f, 0.0f },
    (vertex_texcoord_t){ 100, get_window_height()-100, 0.0f, 1.0f, 1.0f, 1.0f},
    (vertex_texcoord_t){ 100, get_window_height()-00, 0.0f,  1.0f, 1.0f, 0.0f},
    mesh_texture
    );

    // Clear the array of triangles to render every frame loop

#if defined(DYNAMIC_MEM_EACH_FRAME)
    array_free(triangles_to_render);
    array_free(lines_to_render);
#endif

    moveto(10,10);
    gprintf("vertexTime:%d, my func: %d, pikuma: %d. ms/ms2=%f\n", vertex_time, time1, time2, (double)timediff);
    gprintf("frame %d, fps:%d, culled:%d, trisRender:%d\n", numframes, frames_per_second, num_culled, num_triangles_to_render );
    gprintf("1,2,3,4,5,6, 1:wire points, 2:wire, 3:fill 4:fill wire, 5:tex, 6:tex wire\n");
    gprintf("c - cull_method=%d\n", cull_method);
    gprintf("d - disable cull=%d\n", cull_method);
    gprintf("z - sort faces by Z=%d\n", sort_faces_by_z_enable);
    gprintf("n - display_normals_enable=%d\n", display_normals_enable);
    gprintf("t - torb=%d\n", draw_triangles_torb);
    gprintf("num_culled=%d, not culled:%d, tris rendered:%d\n", num_culled, num_not_culled, num_triangles_to_render);
    gprintf("num_cull_zero_area=%d, cull_small:%d\n", num_cull_zero_area, num_cull_small_area);
    gprintf("num_cull_near=%d, far=%d, xy:%d\n", num_cull_near, num_cull_far, num_cull_xy);
    gprintf("num_cull_few=%d, many:%d\n", num_cull_few, num_cull_many);
    gprintf("num_cull_backface=%d, num_cull_degenerate(after clip)=%d\n", num_cull_backface, num_cull_degenerate);


    render_color_buffer();
}

void free_resources(void) {
    free_png_texture();
#if defined(DYNAMIC_MEM_EACH_FRAME)
#else
    sb_free(triangles_to_render);
#endif
    free(color_buffer);
    free(z_buffer);
    free_mesh(&mesh);
}

int main(int argc, char *argv[])
{
    SDL_Log("Hello courses.pikuma.com ! Build %s at %s\n", __DATE__, __TIME__);
    const char* mesh_file = "./assets/cube.obj";
    const char* texture_file = "./assets/cube.png";
    for(int i=0; i<argc; i++)
    {
        if (i==0) continue;
        if (EndsWith(argv[i], "obj") ) {
            mesh_file = argv[i];
        }
        if (EndsWith(argv[i], "png") ) {
            texture_file = argv[i];
        }
    }

    SDL_Log("try to load %s and %s", mesh_file, texture_file);

    is_running = init_window();
    if (is_running) pk_init( color_buffer, z_buffer, get_window_width(), get_window_height() );

    setup(mesh_file, texture_file);

    while (is_running) {
        process_input();
        update();
        vertexShading(uniforms.model_matrix, uniforms.view_matrix, uniforms.projection_matrix);
        render();
    }
    destroy_window();
    free_resources();
    SDL_Log("App closed.");
    return EXIT_SUCCESS;
}
