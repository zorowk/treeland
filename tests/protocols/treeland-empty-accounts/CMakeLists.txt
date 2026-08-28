treeland_add_protocol_test(
    NAME treeland_empty_accounts
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-empty-accounts.c"
)

set_property(TEST test_treeland_empty_accounts APPEND PROPERTY ENVIRONMENT
    "TREELAND_PROTOCOL_TEST_EMPTY_ACCOUNTS=1")

treeland_add_protocol_test(
    NAME treeland_accounts_service_unavailable
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/treeland-empty-accounts.c"
)

set_property(TEST test_treeland_accounts_service_unavailable APPEND PROPERTY ENVIRONMENT
    "TREELAND_PROTOCOL_TEST_NO_ACCOUNTS_SERVICE=1")
