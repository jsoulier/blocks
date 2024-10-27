#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <gfx/atlas.h>
#include <gfx/gfx.h>
#include "camera.h"
#include "noise.h"
#include "world.h"

static SDL_Window* window;
static SDL_GPUTexture* color;
static SDL_GPUTexture* depth;
static uint32_t width;
static uint32_t height;
static camera_t camera;

static void draw()
{
    if (!gfx_begin_frame())
    {
        return;
    }
    gfx_bind_pipeline(PIPELINE_VOXEL);
    camera_update(&camera);
    gfx_push_uniform(PIPELINE_VOXEL_MVP, camera.matrix);
    world_render(&camera, gfx_get_commands(), gfx_get_pass());
    gfx_end_frame();
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    SDL_SetAppMetadata("blocks", NULL, NULL);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    window = SDL_CreateWindow("blocks", 1024, 764, 0);
    if (!window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    gfx_init(window);
    noise_init(NOISE_CUBE);
    if (!world_init(gfx_get_device()))
    {
        return EXIT_FAILURE;
    }
    SDL_SetWindowResizable(window, true);
    SDL_Surface* icon = atlas_get_icon(BLOCK_GRASS, DIRECTION_N);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);

    camera_init(&camera);
    camera_move(&camera, 0, 30, 0);

    int loop = 1;
    while (loop)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                loop = 0;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                camera_size(&camera, event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (SDL_GetWindowRelativeMouseMode(window))
                {
                    float sensitivity = 0.1f;
                    camera_rotate(&camera, -event.motion.yrel * sensitivity, event.motion.xrel * sensitivity);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                SDL_SetWindowRelativeMouseMode(window, 1);
                if (event.button.button == SDL_BUTTON_LEFT) {
                    float s, t, p;
                    camera_vector(&camera, &s, &t, &p);
                    float length = sqrtf(s * s + t * t + p * p);
                    s /= length;
                    t /= length;
                    p /= length;
                    float a, b, c;
                    for (float i = 0; i < 10.0f; i += 0.0001f) {
                        float x, y, z;
                        camera_position(&camera, &x, &y, &z);
                        a = x + s * i;
                        b = y + t * i;
                        c = z + p * i;
                        block_t block = world_get_block(a, b, c);
                        if (block) {
                            world_set_block(a, b, c, BLOCK_EMPTY);
                            // SDL_Log("%d", block);
                            break;
                        }
                    }
                }
                break;

            // case SDL_EVENT_MOUSE_BUTTON_DOWN:
            //     SDL_SetWindowRelativeMouseMode(window, 1);  // Keep original value
            //     if (event.button.button == SDL_BUTTON_LEFT) {
            //         float s, t, p;
            //         camera_vector(&camera, &s, &t, &p); // Get camera direction vector

            //         float length = sqrtf(s * s + t * t + p * p);
            //         s /= length;
            //         t /= length;
            //         p /= length;

            //         // Get the camera's position
            //         float x, y, z;
            //         camera_position(&camera, &x, &y, &z);

            //         // Step values for traversing the ray
            //         int step_x = (s > 0) ? 1 : -1;
            //         int step_y = (t > 0) ? 1 : -1;
            //         int step_z = (p > 0) ? 1 : -1;

            //         // Calculate tmax and tdelta
            //         float tmax_x = ((s > 0) ? ceilf(x) : floorf(x)) - x; // Time to the next x boundary
            //         float tmax_y = ((t > 0) ? ceilf(y) : floorf(y)) - y; // Time to the next y boundary
            //         float tmax_z = ((p > 0) ? ceilf(z) : floorf(z)) - z; // Time to the next z boundary

            //         // Calculate tdelta (time to move one unit)
            //         float tdelta_x = (s == 0) ? INFINITY : (step_x / s); // Avoid division by zero
            //         float tdelta_y = (t == 0) ? INFINITY : (step_y / t);
            //         float tdelta_z = (p == 0) ? INFINITY : (step_z / p);

            //         // Maximum distance for raycasting
            //         float max_distance = 10.0f; // or any other defined limit

            //         while (true) {
            //             // Check for block existence
            //             block_t block = world_get_block((int)ceilf(x), (int)ceilf(y), (int)ceilf(z));
            //             if (block) {
            //                 // Remove the block
            //                 world_set_block((int)ceilf(x), (int)ceilf(y), (int)ceilf(z), BLOCK_EMPTY);
            //                 // Uncomment for debugging
            //                 // SDL_Log("Removed block at (%d, %d, %d)", (int)ceilf(x), (int)ceilf(y), (int)ceilf(z));
            //                 break; // Exit after removing the first block
            //             }

            //             // Determine the next voxel to step into
            //             if (tmax_x < tmax_y) {
            //                 if (tmax_x < tmax_z) {
            //                     if (tmax_x > max_distance) break; // Exceeded max distance
            //                     x += step_x; // Move to the next voxel in the x direction
            //                     tmax_x += tdelta_x; // Update the time to the next x boundary
            //                 } else {
            //                     if (tmax_z > max_distance) break; // Exceeded max distance
            //                     z += step_z; // Move to the next voxel in the z direction
            //                     tmax_z += tdelta_z; // Update the time to the next z boundary
            //                 }
            //             } else {
            //                 if (tmax_y < tmax_z) {
            //                     if (tmax_y > max_distance) break; // Exceeded max distance
            //                     y += step_y; // Move to the next voxel in the y direction
            //                     tmax_y += tdelta_y; // Update the time to the next y boundary
            //                 } else {
            //                     if (tmax_z > max_distance) break; // Exceeded max distance
            //                     z += step_z; // Move to the next voxel in the z direction
            //                     tmax_z += tdelta_z; // Update the time to the next z boundary
            //                 }
            //             }
            //         }
            //     }
            //     break;


            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_ESCAPE)
                {
                    SDL_SetWindowRelativeMouseMode(window, 0);
                }
                break;
            }
        }

        float dx = 0.0f;
        float dy = 0.0f;
        float dz = 0.0f;
        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_D]) dx++;
        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A]) dx--;
        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_E]) dy++;
        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_Q]) dy--;
        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_W]) dz++;
        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_S]) dz--;
        float speed = 0.2f;
        dx *= speed;
        dy *= speed;
        dz *= speed;

        camera_move(&camera, dx, dy, dz);

        float a, b, c;
        camera_vector(&camera, &a, &b, &c);
        // SDL_Log("direction = %f, %f, %f", a, b, c);


        float x, y, z;
        camera_position(&camera, &x, &y, &z);
        // SDL_Log("position = %f, %f, %f", x, y, z);

        world_update(x, y, z);

        draw();
    }

    world_free();
    gfx_free();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}