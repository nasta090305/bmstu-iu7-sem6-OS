
#define SERV_PORT 8082
#define BUF_SIZE 200
#define ARR_SIZE 26

typedef enum {GET_SEATS, RESERVE_SEAT} request_type_t;
typedef enum {OK, ALREADLY_RESERVED, ERROR} response_type_t;

typedef union 
{
    struct
    {
        request_type_t action;
        unsigned seat;
    } request;

    struct
    {
        response_type_t status;
        int seats[ARR_SIZE];

    } response;
} req_info_t;
