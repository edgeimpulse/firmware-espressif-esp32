#include "ei_at_server.h"

/* 
 * The definition of get_instance method is here,
 * because AT Server is designed as a singleton class
 * therefore unit testing of this pattern is pretty difficult.
 * To avoid such issue, for the unit test of all functionalities one
 * can exclude this file and add a custom definition which is implementing
 * non-singleton class - returns a new instance for each call of get_instance
 */

ATServer *ATServer::get_instance()
{
    return ATServer::get_instance(nullptr, 0, default_history_size);
}

ATServer *ATServer::get_instance(ATCommand_t *commands, size_t length, size_t max_history_size)
{
    static ATServer instance(commands, length, max_history_size);

    return &instance;
}
