#ifndef __EI_CONSTANTS__H__
#define __EI_CONSTANTS__H__

#define EI_CLASSIFIER_RESIZE_NONE                0
#define EI_CLASSIFIER_RESIZE_FIT_SHORTEST        1
#define EI_CLASSIFIER_RESIZE_FIT_LONGEST         2
#define EI_CLASSIFIER_RESIZE_SQUASH              3

// This exists for linux runner, etc
__attribute__((unused)) static const char *EI_RESIZE_STRINGS[] = { "none", "fit-shortest", "fit-longest", "squash" };

#endif  //!__EI_CONSTANTS__H__