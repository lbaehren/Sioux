// Please note that the content of this file is confidential or protected by law.
// Any unauthorised copying or unauthorised distribution of the information contained herein is prohibited.

#include "unittest++/unittest++.h"
#include "server/connection.h"
#include "server/test_traits.h"
#include "server/test_request_texts.h"

TEST(read_simple_header)
{
    using namespace server::test;

    traits<>::connection_type   socket(begin(simple_get_11), end(simple_get_11), 5);
    traits<>                    trait;

    server::create_connection(socket, trait);

    for (; socket.process(); )
        ;

    CHECK_EQUAL(1u, trait.requests().size());
}

TEST(read_multiple_header)
{
    using namespace server::test;

    traits<>::connection_type   socket(begin(simple_get_11), end(simple_get_11), 400, 2000);
    traits<>                    traits;

    server::create_connection(socket, traits);

    for (; socket.process(); )
        ;

    CHECK_EQUAL(2000u, traits.requests().size());
}

TEST(read_big_buffer)
{
    using namespace server::test;

    std::vector<char>   input;
    for ( unsigned i = 0; i != 1000; ++i )
        input.insert(input.end(), begin(simple_get_11), end(simple_get_11));

    typedef traits<server::test::socket<std::vector<char>::const_iterator> > socket_t;
    socket_t::connection_type   socket(input.begin(), input.end());
    traits<>                    traits;

    server::create_connection(socket, traits);

    for (; socket.process(); )
        ;

    CHECK_EQUAL(1000u, traits.requests().size());
}

TEST(read_buffer_overflow)
{
    using namespace server::test;

    const char header[] = "Accept-Encoding: gzip\r\n";

    std::vector<char>   input(begin(request_without_end_line), end(request_without_end_line));
    for ( unsigned i = 0; i != 10000; ++i )
        input.insert(input.end(), begin(header), end(header));

    typedef traits<server::test::socket<std::vector<char>::const_iterator> > socket_t;
    socket_t::connection_type   socket(input.begin(), input.end());
    traits<>                    traits;

    server::create_connection(socket, traits);

    for (; socket.process(); )
        ;

    CHECK_EQUAL(1u, traits.requests().size());
    CHECK_EQUAL(server::request_header::buffer_full, traits.requests().front()->state());
}

/**
 * @brief sender closed connection, if request was ok, a response should have been send
 */
TEST(close_after_sender_closed)
{
    using namespace server::test;

    traits<>::connection_type   socket(begin(simple_get_11), end(simple_get_11), 0);
    traits<>                    trait;

    boost::weak_ptr<server::connection<traits<>, traits<>::connection_type> > connection(server::create_connection(socket, trait));
    CHECK(!connection.expired());

    while ( socket.process() ) 
        socket.process();

    CHECK_EQUAL("Hello", socket.output());
    CHECK(connection.expired());
}

/**
 * @test client requests a close via connection header
 */
TEST(closed_by_connection_header)
{
    using namespace server::test;

    traits<>::connection_type   socket(begin(simple_get_11_with_close_header), end(simple_get_11_with_close_header), 0);
    traits<>                    trait;

    boost::weak_ptr<server::connection<traits<>, traits<>::connection_type> > connection(server::create_connection(socket, trait));
    CHECK(!connection.expired());

    // two calls to process to receive one he
    CHECK(socket.process());
    CHECK(socket.process());

    CHECK_EQUAL("Hello", socket.output());
    CHECK(connection.expired());
}