
#include "EventSubscriptionTest.h"
#include "EventSubscription.h"
#include "SymbolCodec.h"
#include "ParserCommon.h"

static int last_event_type = 0;
static dx_const_string_t last_symbol = NULL;
static int visit_count = 0;

void dummy_listener (int event_type, dx_const_string_t symbol_name,
                     const dx_event_data_t* data, int data_count) {
    ++visit_count;
    last_event_type = event_type;
    last_symbol = symbol_name;
}

bool event_subscription_test (void) {
    dxf_subscription_t sub1;
    dxf_subscription_t sub2;
    dx_const_string_t large_symbol_set[] = { L"SYMA", L"SYMB", L"SYMC" };
    dx_const_string_t middle_symbol_set[] = { L"SYMB", L"SYMD" };
    dx_const_string_t small_symbol_set[] = { L"SYMB" };
    dx_int_t symbol_code;
    
    if (dx_init_symbol_codec() != R_SUCCESSFUL) {
        return false;
    }
    
    sub1 = dx_create_event_subscription(DX_ET_TRADE | DX_ET_QUOTE);
    sub2 = dx_create_event_subscription(DX_ET_QUOTE);
    
    if (sub1 == dx_invalid_subscription || sub2 == dx_invalid_subscription) {
        return false;
    }
    
    if (!dx_add_symbols(sub1, large_symbol_set, 3)) {
        return false;
    }
    
    if (!dx_add_symbols(sub2, middle_symbol_set, 2)) {
        return false;
    }
    
    // sub1 - SYMA, SYMB, SYMC; QUOTE | TRADE
    // sub2 - SYMB, SYMD; QUOTE
    
    if (!dx_add_listener(sub1, dummy_listener)) {
        return false;
    }
    
    if (!dx_add_listener(sub2, dummy_listener)) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMB");
    
    if (!dx_process_event_data(DX_ET_QUOTE, L"SYMB", symbol_code, NULL, 5)) {
        return false;
    }
    
    // both sub1 and sub2 should receive the data
    
    if (last_event_type != DX_ET_QUOTE || wcscmp(last_symbol, L"SYMB") || visit_count != 2) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMZ");
    
    // unknown symbol SYMZ must be rejected
    
    if (dx_process_event_data(DX_ET_TRADE, L"SYMZ", symbol_code, NULL, 5)) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMD");

    if (!dx_process_event_data(DX_ET_TRADE, L"SYMD", symbol_code, NULL, 5)) {
        return false;
    }
    
    // SYMD is a known symbol to sub2, but sub2 doesn't support TRADEs
    
    if (last_event_type != DX_ET_QUOTE || wcscmp(last_symbol, L"SYMB") || visit_count != 2) {
        return false;
    }
    
    if (!dx_remove_symbols(sub1, small_symbol_set, 1)) {
        return false;
    }
    
    // sub1 is no longer receiving SYMBs
    
    symbol_code = dx_encode_symbol_name(L"SYMB");
    
    if (!dx_process_event_data(DX_ET_QUOTE, L"SYMB", symbol_code, NULL, 5)) {
        return false;
    }
    
    // ... but sub2 still does
    
    if (last_event_type != DX_ET_QUOTE || wcscmp(last_symbol, L"SYMB") || visit_count != 3) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMA");
    
    if (!dx_process_event_data(DX_ET_TRADE, L"SYMA", symbol_code, NULL, 5)) {
        return false;
    }
    
    // SYMA must be processed by sub1
    
    if (last_event_type != DX_ET_TRADE || wcscmp(last_symbol, L"SYMA") || visit_count != 4) {
        return false;
    }
    
    if (!dx_remove_listener(sub2, dummy_listener)) {
        return false;
    }
    
    symbol_code = dx_encode_symbol_name(L"SYMB");

    // SYMB is still supported by sub2, but sub2 no longer has a listener
    
    if (!dx_process_event_data(DX_ET_QUOTE, L"SYMB", symbol_code, NULL, 5)) {
        return false;
    }
    
    if (last_event_type != DX_ET_TRADE || wcscmp(last_symbol, L"SYMA") || visit_count != 4) {
        return false;
    }
    
    if (!dx_close_event_subscription(sub1)) {
        return false;
    }
    
    if (!dx_close_event_subscription(sub2)) {
        return false;
    }
    
    return true;
}