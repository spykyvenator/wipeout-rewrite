#include "../mem.h"
#include "../utils.h"
#include "../types.h"
#include "../render.h"
#include "../system.h"

#include "object.h"
#include "track.h"
#include "ship.h"
#include "weapon.h"
#include "droid.h"
#include "camera.h"
#include "game.h"

void camera_init(camera_t *camera, section_t *section) {
	camera->section = section;
	for (int i = 0; i < 10; i++) {
		camera->section = camera->section->next;
	}

	camera->position = camera->section->center;
	camera->velocity = vec3(0, 0, 0);
	camera->angle = vec3(0, 0, 0);
        camera->swiv = vec3(0, 0, 0);
	camera->angular_velocity = vec3(0, 0, 0);
	camera->has_initial_section = false;
        camera->shake_start = 0;
}

vec3_t camera_forward(camera_t *camera) {
	float sx = sin(camera->angle.x);
	float cx = cos(camera->angle.x);
	float sy = sin(camera->angle.y);
	float cy = cos(camera->angle.y);
	return vec3(-(sy * cx), -sx, (cy * cx));
}

void camera_update_swivel(camera_t *camera) {
        if (!camera->shake_start) return;
        float time = system_time();
        //float rand = ((float) rand_int(-1000, 1000))/1000;
        //float sinus = sin(fmod(t, .2)/.5);
        double s = ((time - camera->shake_start)  * camera->shake_frequency);
        int s0 = (int) s + 1;
        int s1 = s0 + 1;
        float decay = time >= camera->shake_start+camera->shake_duration ?
                0 : (camera->shake_duration - time)*5 / camera->shake_duration;
        if (decay <= 0) {camera->shake_start = 0; puts("swivel end");}

        camera->swiv = vec3((camera->samplesx[s0] + (s-s0) * camera->samplesx[s1] - camera->samplesx[s0])*(decay/20), (camera->samplesy[s0] + (s-s0) * camera->samplesy[s1] - camera->samplesy[s0])*(decay/20), 0);

        printf("s: %f, s0: %d, s1: %d, start: %f, decay; %f\n", s, s0, s1, camera->shake_start, decay);
        printf("%f, %f, %f\n", camera->swiv.x, camera->swiv.y, camera->swiv.z);
}

void camera_update(camera_t *camera, ship_t *ship, droid_t *droid) {
        camera_update_swivel(camera);
	camera->last_position = camera->position;
	(camera->update_func)(camera, ship, droid);
	camera->real_velocity = vec3_mulf(vec3_sub(camera->position, camera->last_position), 1.0/system_tick());
}

void camera_update_race_external(camera_t *camera, ship_t *ship, droid_t *droid) {
	vec3_t pos = vec3_sub(ship->position, vec3_mulf(ship->dir_forward, 1024));
	pos.y -= 200;
	camera->section = track_nearest_section(pos, camera->section, NULL);
	section_t *next = camera->section->next;

	vec3_t target = vec3_project_to_ray(pos, next->center, camera->section->center);

	vec3_t diff_from_center = vec3_sub(pos, target);
	vec3_t acc = diff_from_center;
	acc.y += vec3_len(diff_from_center) * 0.5;
	
	camera->velocity = vec3_sub(camera->velocity, vec3_mulf(acc, 0.015625 * 30 * system_tick()));
	camera->velocity = vec3_sub(camera->velocity, vec3_mulf(camera->velocity, 0.125 * 30 * system_tick()));
	pos = vec3_add(pos, camera->velocity);

	camera->position = pos;
	camera->angle = vec3(ship->angle.x + camera->swiv.x, ship->angle.y + camera->swiv.y, 0);
}

void init_shake(camera_t *camera, int duration, int frequency) {
        double t = system_time();

        camera->shake_start = t;
        camera->shake_duration = t + duration;
        camera->sample_count = (duration) * frequency;
        camera->shake_frequency = frequency;

        if (camera->sample_count > sizeof(camera->samplesx)/sizeof(double))
                return;
        for (int i = 0; i < camera->sample_count; i++) {
                camera->samplesx[i] = ((double)rand() / RAND_MAX) * 2 - 1;
                camera->samplesy[i] = ((double)rand() / RAND_MAX) * 2 - 1;
        }
        puts("shaking\n");
}

void camera_update_race_internal(camera_t *camera, ship_t *ship, droid_t *droid) {
	camera->section = ship->section;
	camera->position = ship_cockpit(ship);
	camera->angle = vec3(ship->angle.x + camera->swiv.x, ship->angle.y + camera->swiv.y, ship->angle.z * save.internal_roll + camera->swiv.z);
}

void camera_update_race_intro(camera_t *camera, ship_t *ship, droid_t *droid) {
	// Set to final position
	vec3_t pos = vec3_sub(ship->position, vec3_mulf(ship->dir_forward, 0.25 * 4096));

	pos.x += sin(( (ship->update_timer - UPDATE_TIME_RACE_VIEW) * 30 * 3.0 * M_PI * 2) / 4096.0) * 4096;
	pos.y -= (2 *  (ship->update_timer - UPDATE_TIME_RACE_VIEW) * 30) + 200;
	pos.z += sin(( (ship->update_timer - UPDATE_TIME_RACE_VIEW) * 30 * 3.0 * M_PI * 2) / 4096.0) * 4096;

	if (!camera->has_initial_section) {
		camera->section = ship->section;
		camera->has_initial_section = true;
	}
	else {
		camera->section = track_nearest_section(pos, camera->section, NULL);
	}

	camera->position = pos;
	camera->angle.z = 0;
	camera->angle.x = ship->angle.x * 0.5;
	vec3_t target = vec3_sub(ship->position, pos);

	camera->angle.y = -atan2(target.x, target.z);

	if (ship->update_timer <= UPDATE_TIME_RACE_VIEW) {
		flags_add(ship->flags, SHIP_VIEW_INTERNAL);
		camera->update_func = camera_update_race_internal;
	}
}

void camera_update_attract_circle(camera_t *camera, ship_t *ship, droid_t *droid) {
	camera->update_timer -= system_tick();
	if (camera->update_timer <= 0) {
		camera->update_func = camera_update_attract_random;
	}
	// FIXME: not exactly sure what I'm doing here. The PSX version behaves
	// differently.
	camera->section = ship->section;

	camera->position.x = ship->position.x + sin(ship->angle.y) * 512;
	camera->position.y = ship->position.y + ((ship->angle.x * 512 / (M_PI * 2)) - 200);
	camera->position.z = ship->position.z - cos(ship->angle.y) * 512;

	camera->position.x += sin(camera->update_timer * 0.25) * 512;
	camera->position.y -= 400;
	camera->position.z += cos(camera->update_timer * 0.25) * 512;
	camera->position = vec3_sub(camera->position, vec3_mulf(ship->dir_up, 256));

	vec3_t target = vec3_sub(ship->position, camera->position);
	float height = sqrt(target.x * target.x + target.z * target.z);
	camera->angle.x = -atan2(target.y, height);
	camera->angle.y = -atan2(target.x, target.z);
}

void camera_update_rescue(camera_t *camera, ship_t *ship, droid_t *droid) {
	camera->position = vec3_add(camera->section->center, vec3(300, -1500, 300));

	vec3_t target = vec3_sub(droid->position, camera->position);
	float height = sqrt(target.x * target.x + target.z * target.z);
	camera->angle.x = -atan2(target.y, height);
	camera->angle.y = -atan2(target.x, target.z);
}


void camera_update_attract_internal(camera_t *camera, ship_t *ship, droid_t *droid) {
	camera->update_timer -= system_tick();
	if (camera->update_timer <= 0) {
		camera->update_func = camera_update_attract_random;
	}

	camera->section = ship->section;
	camera->position = ship_cockpit(ship);
	camera->angle = vec3(ship->angle.x, ship->angle.y, 0); // No roll
}

void camera_update_static_follow(camera_t *camera, ship_t *ship, droid_t *droid) {
	camera->update_timer -= system_tick();
	if (camera->update_timer <= 0) {
		camera->update_func = camera_update_attract_random;
	}

	vec3_t target = vec3_sub(ship->position, camera->position);
	float height = sqrt(target.x * target.x + target.z * target.z);
	camera->angle.x = -atan2(target.y, height);
	camera->angle.y = -atan2(target.x, target.z);
}

void camera_update_attract_random(camera_t *camera, ship_t *ship, droid_t *droid) {
	flags_rm(ship->flags, SHIP_VIEW_INTERNAL);

	if (rand() % 2) {
		camera->update_func = camera_update_attract_circle;
		camera->update_timer = 5;
	}
	else {
		camera->update_func = camera_update_static_follow;
		camera->update_timer = 5;
		section_t *section = ship->section->next;
		for (int i = 0; i < 10; i++) {
			section = section->next;
		}

		camera->section = section;
		camera->position = section->center;
		camera->position.y -= 500;
	}

	(camera->update_func)(camera, ship, droid);
}
