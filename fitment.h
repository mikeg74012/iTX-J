/**
 **************************************************************************************************
 * @file   fitment.h
 **************************************************************************************************
 *
 * @author Ripcord Designs
 *
 * @brief  Motion Monitor buffers for Accelerometer, Gyro and GNSS data.
 *
 **************************************************************************************************
 */

#ifndef FITMENT_H
#define FITMENT_H
#include <stdbool.h>

#include "main.h"
#include "motionbuffer.h"

/*
 **************************************************************************************************
 *                                         PUBLIC DEFINES
 **************************************************************************************************
 */

typedef enum
{
    SURVEY_METHOD_PEAK,
    SURVEY_METHOD_AVERAGE,
} SurveyMethod_t;

#define         FITMENT_SURVEY_INTERVAL             10          /* Every 10s for each survey data*/

#define         MOUNT_ANGLE_DEFAULT                 (((0x80|45)<<8) + (0x80|1))    // unit mount at -45, -1 degree
#define         MOUNT_PARAM_DEFAULT                 0                              /* mount survey related param */
#define         SURVEY_DEFAULT_SPEED_THRESHOLD      100         /* (speed from GNSS x 0.1) */
#define         SURVEY_DEFAULT_COLLECT_DURATION     10          /* Unit is in Mins*/
#define         SURVEY_DEFAULT_METHOD               SURVEY_METHOD_PEAK

#define         MNTSURVEY_SETTING_DEFAULT           ((SURVEY_DEFAULT_SPEED_THRESHOLD & 0xFF) | \
                                                        ((SURVEY_DEFAULT_COLLECT_DURATION & 0xFF) << 8) | \
                                                        ((SURVEY_DEFAULT_METHOD & 0xFF) << 16) | \
                                                        (0x00 << 24))

/* Action to be used for the function of FitmentDataHandler*/
#define MOUNT_SURVEY_BACKUP             20           /* Backup current (active) survey to pages 0 */
#define MOUNT_SURVEY_IMPORT             21           /* Import survey from page 0 */
#define MOUNT_SURVEY_DATA_RESET         0            /* Reset Survey Data */


/*
 ******************************************************************************
 *                  PUBLIC FUNCTION PROTOTYPES / INTERFACE                    *
 ******************************************************************************
 */

/* mounting correction process */
void            FitmentNormalisationEnable          (bool enabled);
void            FitmentActivateMountCheck           (void);
bool            FitmentDataHandler                  (uint8_t mode);

bool            IsFitmentNormalizationOn            (void);
bool            IsScreenMountedPreviously           (void);
bool            IsMountCheckActive                  (void);

void            FitmentApplyRotationToIMU(ImuData_t *ImuData, int pitch, int roll);
void            FitmentMountSurveyUpdate(const ImuData_t *ImuData, int pitch, int roll);
uint8_t         FitmentMatrixInit                    (void);

/* Console Commands */
int             FitmentConsoleRmpConfig      (int ArgCount, char **pArguments);
#endif /* FITMENT_H */
