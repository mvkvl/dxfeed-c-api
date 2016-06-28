
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <string.h>
#include <wctype.h>
#include <stdlib.h>
#define stricmp strcasecmp
#endif

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include <stdio.h>
#include <time.h>

typedef int bool;

#define true 1
#define false 0

#define STATIC_PARAMS_COUNT 3


/* -------------------------------------------------------------------------- */
#ifdef _WIN32
static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate() {
    bool res;
    EnterCriticalSection(&listener_thread_guard);
    res = is_listener_thread_terminated;
    LeaveCriticalSection(&listener_thread_guard);

    return res;
}
#else
static volatile bool is_listener_thread_terminated = false;
bool is_thread_terminate() {
    bool res;
    res = is_listener_thread_terminated;
    return res;
}
#endif


/* -------------------------------------------------------------------------- */

#ifdef _WIN32
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
    EnterCriticalSection(&listener_thread_guard);
    is_listener_thread_terminated = true;
    LeaveCriticalSection(&listener_thread_guard);

    wprintf(L"\nTerminating listener thread\n");
}
#else
void on_reader_thread_terminate(dxf_connection_t connection, void* user_data) {
    is_listener_thread_terminated = true;
    wprintf(L"\nTerminating listener thread\n");
}
#endif

void print_timestamp(dxf_long_t timestamp){
		wchar_t timefmt[80];
		
		struct tm * timeinfo;
		time_t tmpint = (time_t)(timestamp /1000);
		timeinfo = localtime ( &tmpint );
		wcsftime(timefmt,80, L"%Y%m%d-%H%M%S" ,timeinfo);
		wprintf(L"%ls",timefmt);
}

dxf_const_string_t dx_event_type_to_string(int event_type) {
    switch (event_type) {
        case DXF_ET_TRADE: return L"Trade";
        case DXF_ET_QUOTE: return L"Quote";
        case DXF_ET_SUMMARY: return L"Summary";
        case DXF_ET_PROFILE: return L"Profile";
        case DXF_ET_ORDER: return L"Order";
        case DXF_ET_TIME_AND_SALE: return L"Time&Sale";
        case DXF_ET_CANDLE: return L"Candle";
        default: return L"";
    }
}

/* -------------------------------------------------------------------------- */

void listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data,
              dxf_event_flags_t flags, int data_count, void* user_data) {
    dxf_int_t i = 0;
    dxf_candle_t* candles = NULL;

    wprintf(L"%ls{symbol=%ls, ", dx_event_type_to_string(event_type), symbol_name);

    if (event_type != DXF_ET_CANDLE)
        return;
    candles = (dxf_candle_t*)data;

    for (; i < data_count; ++i) {
        wprintf(L"time=");
        print_timestamp(candles[i].time);
        wprintf(L", sequence=%d, count=%I64i, ",
            candles[i].sequence,
            candles[i].count);
        wprintf(L"open=%f, high=%f, low=%f, close=%f, ",
            candles[i].open,
            candles[i].high,
            candles[i].low,
            candles[i].close);
        wprintf(L"volume=%I64i, VWAP=%f, bidVolume=%I64i, askVolume=%I64i}\n",
            candles[i].volume,
            candles[i].vwap,
            candles[i].bid_volume,
            candles[i].ask_volume);
    }
}
/* -------------------------------------------------------------------------- */

void process_last_error () {
    int error_code = dx_ec_success;
    dxf_const_string_t error_descr = NULL;
    int res;

    res = dxf_get_last_error(&error_code, &error_descr);

    if (res == DXF_SUCCESS) {
        if (error_code == dx_ec_success) {
            wprintf(L"WTF - no error information is stored");
            return;
        }

        wprintf(L"Error occurred and successfully retrieved:\n"
            L"error code = %d, description = \"%ls\"\n",
            error_code, error_descr);
        return;
    }

    wprintf(L"An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */
dxf_string_t ansi_to_unicode (const char* ansi_str) {
#ifdef _WIN32
    size_t len = strlen(ansi_str);
    dxf_string_t wide_str = NULL;

    // get required size
    int wide_size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, 0);

    if (wide_size > 0) {
        wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, wide_size);
    }

    return wide_str;
#else /* _WIN32 */
    dxf_string_t wide_str = NULL;
    size_t wide_size = mbstowcs(NULL, ansi_str, 0);
    if (wide_size > 0) {
        wide_str = calloc(wide_size + 1, sizeof(dxf_char_t));
        mbstowcs(wide_str, ansi_str, wide_size + 1);
    }

    return wide_str; /* todo */
#endif /* _WIN32 */
}

#define DATE_TIME_BUF_SIZE 4
/*
 * Parse date string in format 'DD-MM-YYYY'
 */
bool parse_date(const char* date_str, struct tm* time_struct) {
    int i;
    int len = strlen(date_str);
    int separator_count = 0;
    char buf[DATE_TIME_BUF_SIZE + 1] = { 0 };
    int buf_len = 0;
    int mday = 0;
    int month = 0;
    int year = 0;
    for (i = 0; i < len; i++) {
        if (date_str[i] == '-') {
            if (separator_count == 0)
                mday = atoi(buf);
            else if (separator_count == 1)
                month = atoi(buf);
            else
                return false;
            separator_count++;
            memset(buf, 0, DATE_TIME_BUF_SIZE);
            continue;
        }

        buf_len = strlen(buf);
        if (buf_len >= DATE_TIME_BUF_SIZE)
            return false;
        buf[buf_len] = date_str[i];
    }

    year = atoi(buf);

    if (mday == 0 || month == 0 || year == 0)
        return false;
    time_struct->tm_mday = mday;
    time_struct->tm_mon = month - 1;
    time_struct->tm_year = year - 1900;
    return true;
}

int main (int argc, char* argv[]) {
    dxf_connection_t connection;
    dxf_subscription_t subscription;
    int loop_counter = 100000;
    int event_type = DXF_ET_CANDLE;
    dxf_candle_attributes_t candle_attributes;
    dxf_string_t symbol = NULL;
    char* dxfeed_host = NULL;
    dxf_string_t dxfeed_host_u = NULL;
    time_t time_value = time(NULL);
    struct tm time_struct;
    struct tm* local_time = localtime(&time_value);
    int i = 0;

    time_struct.tm_sec = 0;
    time_struct.tm_min = 0;
    time_struct.tm_hour = 0;
    time_struct.tm_mday = local_time->tm_mday;
    time_struct.tm_mon = local_time->tm_mon;
    time_struct.tm_year = local_time->tm_year;
    time_value = mktime(&time_struct);

    if (argc < STATIC_PARAMS_COUNT) {
        wprintf(L"DXFeed candle console sample.\n"
            L"Usage: CandleSample <server address> <symbol> [-t DD-MM-YYYY]\n"
            L"  <server address> - a DXFeed server address, e.g. demo.dxfeed.com:7300\n"
            L"  <symbol> - a trade symbol, e.g. C, MSFT, YHOO, IBM\n"
            L"  [-t DD-MM-YYYY] - time which candle started\n");

        return 0;
    }

    dxf_initialize_logger( "log.log", true, true, true );

    dxfeed_host = argv[1];

    symbol = ansi_to_unicode(argv[2]);
    if (symbol == NULL) {
        return -1;
    }
    else {
        for (i = 0; symbol[i]; i++)
            symbol[i] = towupper(symbol[i]);
    }

    if (argc > STATIC_PARAMS_COUNT) {
        for (i = STATIC_PARAMS_COUNT; i < argc; i++) {
            if (strcmp(argv[i], "-t") == 0) {
                if (i + 1 == argc) {
                    wprintf(L"Date argument error\n");
                    return -1;
                }
                i += 1;
                if (!parse_date(argv[i], &time_struct)) {
                    wprintf(L"Date format error\n");
                    return -1;
                }
                time_value = mktime(&time_struct);
            }
        }
    }

    wprintf(L"Sample test started.\n");
    dxfeed_host_u = ansi_to_unicode(dxfeed_host);
    wprintf(L"Connecting to host %ls...\n", dxfeed_host_u);
    free(dxfeed_host_u);

#ifdef _WIN32
    InitializeCriticalSection(&listener_thread_guard);
#endif

    if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, &connection)) {
        free(symbol);
        process_last_error();
        return -1;
    }

    wprintf(L"Connection successful!\n");

    if (!dxf_create_subscription_timed(connection, event_type, time_value, &subscription)) {
        free(symbol);
        process_last_error();
        return -1;
    };

    if (!dxf_initialize_candle_symbol_attributes(symbol, DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT, 
                                                 DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT, dxf_ctpa_day, 
                                                 dxf_cpa_mark, dxf_csa_default, dxf_caa_default, &candle_attributes)) {
        dxf_close_subscription(subscription);
        dxf_close_connection(connection);
        free(symbol);
        process_last_error();
        return -1;
    }

    if (!dxf_add_candle_symbol(subscription, candle_attributes)) {
        dxf_delete_candle_symbol_attributes(candle_attributes);
        dxf_close_subscription(subscription);
        dxf_close_connection(connection);
        free(symbol);
        process_last_error();
        return -1;
    };

    if (!dxf_attach_event_listener(subscription, listener, NULL)) {
        dxf_delete_candle_symbol_attributes(candle_attributes);
        dxf_close_subscription(subscription);
        dxf_close_connection(connection);
        free(symbol);
        process_last_error();
        return -1;
    };
    wprintf(L"Subscription successful!\n");

    while (!is_thread_terminate() && loop_counter--) {
#ifdef _WIN32
        Sleep(100);
#else
		sleep(1);
#endif
    }
    
    wprintf(L"Disconnecting from host...\n");

    if (!dxf_delete_candle_symbol_attributes(candle_attributes)) {
        process_last_error();
        return -1;
    }

    if (!dxf_close_subscription(subscription)) {
        process_last_error();
        return -1;
    }

    if (!dxf_close_connection(connection)) {
        process_last_error();

        return -1;
    }

    free(symbol);

    wprintf(L"Disconnect successful!\nConnection test completed successfully!\n");
    wprintf(L"loops remain:%d\n", loop_counter);

#ifdef _WIN32
    DeleteCriticalSection(&listener_thread_guard);
#endif
    
    return 0;
}
