/**
 * @file main.c
 * @author Adrian Cinal
 * @brief File providing the entry point for the application
 */

#include <boap_router.h>

/**
 * @brief Application entry point
 */
void app_main(void) {

    /* Start the router service */
    (void) BoapRouterInit();
}
