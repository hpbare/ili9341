/**
 * @file    display.h
 * @brief   Public C interface for the ILI9341 display component.
 */
#ifndef DISPLAY_H_
#define DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initialize the ILI9341 LCD panel.
 * @return 0 on success, negative value on error.
 */
int display_init(void);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_H_ */
