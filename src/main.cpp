#include <stdbool.h>
#include <stdint.h>
#include "core.h"

int main(void) {
	Core core;
	core.init();
	core.loop();
}
