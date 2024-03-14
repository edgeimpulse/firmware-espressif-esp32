#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "ei_at_command_set.h"
#include <vector>
#include <string>

bool at_info(void)
{
    std::vector<std::string> sensors = {"unknown", "microphone", "accelerometer", "camera", "9DoF", "environmental", "fusion"};

    ei_printf("*************************\n");
    ei_printf("* Edge Impulse firmware *\n");
    ei_printf("*************************\n");
    ei_printf("Firmware build date  : " __DATE__ "\n");
    ei_printf("Firmware build time  : " __TIME__ "\n");
    ei_printf("ML model author      : " EI_CLASSIFIER_PROJECT_OWNER "\n");
    ei_printf("ML model name        : " EI_CLASSIFIER_PROJECT_NAME "\n");
    ei_printf("ML model ID          : %d\n", EI_CLASSIFIER_PROJECT_ID);
    ei_printf("Model deploy version : %d\n", EI_CLASSIFIER_PROJECT_DEPLOY_VERSION);
    ei_printf("Edge Impulse version : v%d.%d.%d\n", EI_STUDIO_VERSION_MAJOR, EI_STUDIO_VERSION_MINOR, EI_STUDIO_VERSION_PATCH);
    ei_printf("Used sensor          : %s\n", sensors[EI_CLASSIFIER_SENSOR].c_str());

    return true;
}
