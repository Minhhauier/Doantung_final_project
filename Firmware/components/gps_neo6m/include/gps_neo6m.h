#ifndef GPS_NEO6M
#define GPS_NEO6M

#include <stdbool.h>

typedef struct {
    int satellites_in_view;
    int satellites_used;
    int fix_quality;
    double latitude;
    double longitude;
    bool has_fix;
} gps_data_t;

void gps_neo6m_init();
void gps_parse_nmea_line(const char *line, gps_data_t *gps_data);
void gps_parse_nmea(const char *nmea_data, gps_data_t *gps_data);
void gps_print_data(const gps_data_t *gps_data);
void read_gps_data_task(void *pvParameters);

#endif
