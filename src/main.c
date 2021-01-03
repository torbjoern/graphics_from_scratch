#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "func.h"
#include "display.h"
#include "vector.h"
#include "mesh.h"
#include "array.h"
#include "matrix.h"
#include "light.h"
#include "texture.h"

// Review of structs
typedef struct {
    vec3_t position;
} camera_t;

camera_t camera = {
    {0, 0, 0},
};
mat4_t proj_matrix;
mat4_t normal_matrix;

typedef struct {
    vec4_t a,b;
} line_t;

bool sort_faces_by_z_enable = true;
bool display_normals_enable = false;
triangle_t *triangles_to_render = NULL;
line_t* lines_to_render = NULL;


double time = 0;
bool is_running = false;
unsigned int previous_frame_time = 0;

struct mouse_t {
    int x,y;
    bool left,right;
} mouse;

void setup(const char* mesh_file) {
    time = 0.0;

    // Initialize render mode and triangle culling method
    render_method = RENDER_WIRE;
    cull_method = CULL_BACKFACE;

    float aspect_ratio = window_height / (float) window_width;
    float fov_angle = PI / 3.0f;
    proj_matrix = mat4_make_perspective(fov_angle, aspect_ratio, 0.1f, 100.0f);

    //load_cube_mesh_data();
    load_obj_file_data(mesh_file);
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
        }
        break;
        }
    }// while
}

vec4_t to_screen_space(vec4_t v)
{
  v.x *= (window_width / 2);
  v.y *= (window_height / 2);

  // Invert screen y coordinate since our display goes from 0 to window_height
  v.y *= -1;

  v.x += (window_width / 2);
  v.y += (window_height / 2);

  return v;
}


void update(void) {

    // Wait some time until we reach the target frame time
    int time_to_wait =
        (int)(previous_frame_time + FRAME_TARGET_TIME) - SDL_GetTicks();

    time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - (int)previous_frame_time);

    // Only delay if we are too fast
    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) SDL_Delay(time_to_wait);
    previous_frame_time = SDL_GetTicks();

    // Initialize the array of triangles to render
    triangles_to_render = NULL;

    time += 1.0 / (double)FRAME_TARGET_TIME;
    static float mouse_dx = 0;
    static float mouse_dy = 0;
    static float mouse_ox = 0;
    static float mouse_oy = 0;
    mouse_dx = mouse.x - mouse_ox;
    mouse_dy = mouse.y - mouse_oy;
    mouse_ox = mouse.x;
    mouse_oy = mouse.y;
    if (mouse.left) mesh.rotation.x += .01f*mouse_dx;
    if (mouse.right) mesh.rotation.y += .01f*mouse_dy;
    //mesh.rotation.z = 0;

    mesh.rotation.x = .5*time;
    mesh.rotation.y = .5*time;
    mesh.rotation.z = 0;

    mesh.translation.x = 0;
    mesh.translation.y = 0.5f;
    //mesh.translation.z = -40.f + 80.f * mouse.y / (float)window_height;
    mesh.translation.z = 4.f;

    // Create scale, rotation, and translation matrices that will be used to multiply the mesh vertices
    mat4_t scale_matrix = mat4_make_scale(mesh.scale.x, mesh.scale.y, mesh.scale.z);
    mat4_t translation_matrix = mat4_make_translation(mesh.translation.x, mesh.translation.y, mesh.translation.z);
    mat4_t rotation_matrix_x = mat4_make_rotation_x(mesh.rotation.x);
    mat4_t rotation_matrix_y = mat4_make_rotation_y(mesh.rotation.y);
    mat4_t rotation_matrix_z = mat4_make_rotation_z(mesh.rotation.z);
    mat4_t world_matrix = mat4_identity();
    // Order matters: scale, rotate, translate. [T][R][S]*v
    world_matrix = mat4_mul_mat4(scale_matrix, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_x, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_y, world_matrix);
    world_matrix = mat4_mul_mat4(rotation_matrix_z, world_matrix);
    normal_matrix = world_matrix;
    world_matrix = mat4_mul_mat4(translation_matrix, world_matrix);
    const mat4_t mvp_matrix = mat4_mul_mat4(proj_matrix, world_matrix);

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

    const mat4_t light_mvp = mat4_mul_mat4(proj_matrix, light_world_matrix);
    vec4_t light_transformed = mat4_mul_vec4_project(light_mvp, light.position_proj);
    light.position_proj = to_screen_space(light_transformed);


    lines_to_render = NULL;

    // Loop all triangle faces of our mesh
    int n_faces = array_length(mesh.faces);
    for (int i = 0; i < n_faces; i++) {
        face_t mesh_face = mesh.faces[i];

        vec3_t face_vertices[3];
        face_vertices[0] = mesh.vertices[mesh_face.a - 1]; // Minus 1 because mesh vertices start from 1.
        face_vertices[1] = mesh.vertices[mesh_face.b - 1];
        face_vertices[2] = mesh.vertices[mesh_face.c - 1];

        vec2_t face_texcoords[3];
        face_texcoords[0] = mesh.texcoords[mesh_face.texcoord_a - 1]; // Minus 1 because mesh vertices start from 1.
        face_texcoords[1] = mesh.texcoords[mesh_face.texcoord_b - 1];
        face_texcoords[2] = mesh.texcoords[mesh_face.texcoord_c - 1];

        uint32_t face_colors[3];
        face_colors[0] = 0xFFFF0000;//vec3_to_uint32_t(mesh.colors[mesh_face.a - 1]); // Minus 1 because mesh vertices start from 1.
        face_colors[1] = 0xFF00FF00;//vec3_to_uint32_t(mesh.colors[mesh_face.b - 1]);
        face_colors[2] = 0xFF0000FF;//vec3_to_uint32_t(mesh.colors[mesh_face.c - 1]);

        vec3_t center = {0,0,0};
        center = vec3_add(center, face_vertices[0]);
        center = vec3_add(center, face_vertices[1]);
        center = vec3_add(center, face_vertices[2]);
        center = vec3_mul(center, 1.0f / 3.0f);

        // Use a matrix to scale our original vertex
        vec4_t transformed_center = mat4_mul_vec4(mvp_matrix, vec4_from_vec3(center) );

        vec4_t transformed_vertices[3];
        // Loop all three vertices of this current face and apply transformations
        for (int j = 0; j < 3; j++) {
            //vec4_t transformed_vertex = mat4_mul_vec4(world_matrix, vec4_from_vec3(face_vertices[j]) );
            vec4_t transformed_vertex = mat4_mul_vec4_project(mvp_matrix, vec4_from_vec3(face_vertices[j]) );

            // Save transformed vertex in the array of transformed vertices
            transformed_vertices[j] = transformed_vertex;
        }

        triangle_t projected_triangle;
        projected_triangle.z = transformed_center.z;
        for (int j = 0; j < 3; j++) {
            // Scale and translate the projected points to the middle of the screen
            vec4_t projected_point = to_screen_space(transformed_vertices[j]);
            projected_triangle.points[j].x = projected_point.x;
            projected_triangle.points[j].y = projected_point.y;
            projected_triangle.z = projected_point.z;
            projected_triangle.colors[0] = face_colors[0];
            projected_triangle.colors[1] = face_colors[1];
            projected_triangle.colors[2] = face_colors[2];

            projected_triangle.texcoords[j] = face_texcoords[j];
        }

        // Check backface culling
        vec3_t vector_a = vec3_from_vec4(transformed_vertices[0]); /*   A   */
        vec3_t vector_b = vec3_from_vec4(transformed_vertices[1]); /*  / \  */
        vec3_t vector_c = vec3_from_vec4(transformed_vertices[2]); /* C---B */

        // Get the vector subtraction of B-A and C-A
        vec3_t vector_ab = vec3_sub(vector_b, vector_a);
        vec3_t vector_ac = vec3_sub(vector_c, vector_a);
        vec3_t normal = vec3_cross(vector_ab, vector_ac);

        // Find the vector between a point in the triangle and camera origin
        vec3_t camera_ray = vec3_sub(camera.position, vector_a);

        float rayDotNormal = vec3_dot(camera_ray, normal);

        // Bypass the triangles looking away from camera
        bool front_facing = rayDotNormal > 0.0f;

        //if (cull_method == CULL_BACKFACE && front_facing)
        {
            // Save the projected triangle in the array of triangles to render
            if (cull_method == CULL_BACKFACE && !front_facing)
            {
                continue;
            }
            vec3_t normal = vec3_cross(
                                vec3_sub(face_vertices[1],face_vertices[0]),
                                vec3_sub(face_vertices[2],face_vertices[0])
                            );

            vec3_normalize(&normal);
            projected_triangle.center = center;
            projected_triangle.normal = normal;
            normal = vec3_mul(normal, 0.125f);
            array_push(triangles_to_render, projected_triangle);

            vec4_t start = mat4_mul_vec4_project(mvp_matrix, vec4_from_vec3(center) );
            vec4_t end = mat4_mul_vec4_project(mvp_matrix, vec4_from_vec3( vec3_add(center, normal)) );
            line_t projected_line = {.a = to_screen_space(start), .b = to_screen_space(end) };
            array_push(lines_to_render, projected_line);
        }


    }
}

bool getWinding(float x0, float y0, float x1, float y1, float x2, float y2)
{
    vec2_t v0 = {x1-x0, y1-y0};
    vec2_t v1 = {x2-x0, y2-y0};
    float dot = v0.x*v1.x + v0.y*v1.y;
    if (dot > 0) return true;
    return false;
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
    int num_tris = array_length(triangles_to_render);
    qsort(triangles_to_render, num_tris, sizeof(triangle_t), cmpLess);
}

void render(void) {
    clear_color_buffer( packColor(32,32,32) );
    draw_grid();

    vec3_t centerPos = {0,0,0};
    vec3_t camera_dir =
        vec3_sub(camera.position, centerPos);

    vec3_normalize(&camera_dir);

    if (sort_faces_by_z_enable) sort();

    // Loop all projected triangles and render them
    uint32_t color = 0xFFFFFFFF;
    if (light.position_proj.w > 0.1f)
      circle(light.position_proj.x, light.position_proj.y, 20 - light.position_proj.z);
    circle(mouse.x, mouse.y, 10);

    int num_tris = array_length(triangles_to_render);
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
                triangle.points[0].x, triangle.points[0].y, // vertex A
                triangle.points[1].x, triangle.points[1].y, // vertex B
                triangle.points[2].x, triangle.points[2].y, // vertex C
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
              //vertices[i].z = triangle.points[i].z;
              vertices[vtx].u = triangle.texcoords[vtx].x;
              vertices[vtx].v = triangle.texcoords[vtx].y;
            }
            texture_t texture = {.texels = (uint8_t*)&REDBRICK_TEXTURE[0], .width=64, .height=64};
            draw_triangle_textured(vertices, &texture, colors);
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

    if (display_normals_enable)
    {
        color = 0xFF00FF00;
        int num_lines = array_length(lines_to_render);
        for (int i = 0; i < num_lines; i++) {
            line_t line = lines_to_render[i];
            draw_line(line.a.x, line.a.y, line.b.x, line.b.y, color);
        }

    }

    //draw_triangle(300, 100, 50, 400, 500, 700, 0xFF00FF00);

    // Clear the array of triangles to render every frame loop
    array_free(lines_to_render);
    array_free(triangles_to_render);

    render_color_buffer();
}

void free_resources(void) {
    free(color_buffer);
    array_free(mesh.vertices);
    array_free(mesh.texcoords);
    array_free(mesh.faces);
}

int EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int main(int argc, char *argv[])
{
    SDL_Log("Hello courses.pikuma.com\n");
    const char* mesh_file = "./assets/cube.obj";
    if (argc > 1 && EndsWith(argv[1], "obj") )
    {
        mesh_file = argv[1];
    }
    SDL_Log("try to load %s", mesh_file);
    is_running = init_window();

    setup(mesh_file);

    while (is_running) {
        process_input();
        update();
        render();
    }
    destroy_window();
    free_resources();
    SDL_Log("App closed.");
    return EXIT_SUCCESS;
}
