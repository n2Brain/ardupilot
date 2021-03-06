/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <AP_HAL/AP_HAL.h>
#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
#include <AP_Param/AP_Param.h>
#include "AP_Proximity_SITL.h"
#include <stdio.h>

extern const AP_HAL::HAL& hal;

#define PROXIMITY_MAX_RANGE 200
#define PROXIMITY_ACCURACY 0.1

/* 
   The constructor also initialises the proximity sensor. 
*/
AP_Proximity_SITL::AP_Proximity_SITL(AP_Proximity &_frontend,
                                     AP_Proximity::Proximity_State &_state):
    AP_Proximity_Backend(_frontend, _state)
{
    sitl = (SITL::SITL *)AP_Param::find_object("SIM_");
    ap_var_type ptype;
    fence_count = (AP_Int8 *)AP_Param::find("FENCE_TOTAL", &ptype);
    if (fence_count == nullptr || ptype != AP_PARAM_INT8) {
        AP_HAL::panic("Proximity_SITL: Failed to find FENCE_TOTAL");
    }
}

// get distance in meters in a particular direction in degrees (0 is forward, angles increase in the clockwise direction)
bool AP_Proximity_SITL::get_horizontal_distance(float angle_deg, float &distance) const
{
    if (!fence_loader.boundary_valid(fence_count->get(), fence, true)) {
        return false;
    }

    // convert to earth frame
    angle_deg = wrap_360(sitl->state.yawDeg + angle_deg);

    /*
      simple bisection search to find distance. Not really efficient,
      but we can afford the CPU in SITL
     */
    float min_dist = 0, max_dist = PROXIMITY_MAX_RANGE;
    while (max_dist - min_dist > PROXIMITY_ACCURACY) {
        float test_dist = (max_dist+min_dist)*0.5f;
        Location loc = current_loc;
        location_update(loc, angle_deg, test_dist);
        Vector2l vecloc(loc.lat, loc.lng);
        if (fence_loader.boundary_breached(vecloc, fence_count->get(), fence, true)) {
            max_dist = test_dist;
        } else {
            min_dist = test_dist;
        }
    }
    distance = min_dist;
    return true;
}

// update the state of the sensor
void AP_Proximity_SITL::update(void)
{
    load_fence();
    current_loc.lat = sitl->state.latitude * 1.0e7;
    current_loc.lng = sitl->state.longitude * 1.0e7;
    current_loc.alt = sitl->state.altitude * 1.0e2;
    if (fence && fence_loader.boundary_valid(fence_count->get(), fence, true)) {
        set_status(AP_Proximity::Proximity_Good);
    } else {
        set_status(AP_Proximity::Proximity_NoData);        
    }
}

void AP_Proximity_SITL::load_fence(void)
{
    uint32_t now = AP_HAL::millis();
    if (now - last_load_ms < 1000) {
        return;
    }
    last_load_ms = now;
    
    if (fence == nullptr) {
        fence = (Vector2l *)fence_loader.create_point_array(sizeof(Vector2l));
    }
    if (fence == nullptr) {
        return;
    }
    for (uint8_t i=0; i<fence_count->get(); i++) {
        fence_loader.load_point_from_eeprom(i, fence[i]);
    }
}
#endif // CONFIG_HAL_BOARD
