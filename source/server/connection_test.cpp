// Copyright (c) Torrox GmbH & Co KG. All rights reserved.
// Please note that the content of this file is confidential or protected by law.
// Any unauthorised copying or unauthorised distribution of the information contained herein is prohibited.

#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind.hpp>

#include "server/connection.h"
#include "server/test_traits.h"
#include "server/test_tools.h"
#include "server/test_socket.h"
#include "server/test_timer.h"
#include "http/test_request_texts.h"

using namespace server::test;
using namespace http::test; 

/**
 * @class server::connection
 * @test test the a simple request will be received
 */
BOOST_AUTO_TEST_CASE( read_simple_header )
{
    boost::asio::io_service     queue;
    traits<>::connection_type   socket( queue, begin( simple_get_11 ), end( simple_get_11 ), 5u );
    traits<>                    trait;

    server::create_connection(socket, trait);

    queue.run(); 

    BOOST_CHECK_EQUAL(1u, trait.requests().size());
}

/**
 * @class server::connection
 * @test test that 2000 simple get request will be received
 */
BOOST_AUTO_TEST_CASE( read_multiple_header )
{
    boost::asio::io_service     queue;
    traits<>::connection_type   socket(queue, begin(simple_get_11), end(simple_get_11), 400, 2000);
    traits<>                    traits;

    server::create_connection(socket, traits);

    queue.run();

    BOOST_CHECK_EQUAL(2000u, traits.requests().size());
}

/**
 * @class server::connection
 * @test simulate that the ip stack contains 1000 GET requests at once.
 */
BOOST_AUTO_TEST_CASE( read_big_buffer )
{
    std::vector<char>   input;
    for ( unsigned i = 0; i != 1000; ++i )
        input.insert(input.end(), begin(simple_get_11), end(simple_get_11));

    typedef traits< server::test::response_factory, server::test::socket<std::vector<char>::const_iterator> > socket_t;
    boost::asio::io_service     queue;
    socket_t::connection_type   socket( queue, input.begin(), input.end() );
    traits<>                    traits;

    server::create_connection(socket, traits);

    queue.run();

    BOOST_CHECK_EQUAL(1000u, traits.requests().size());
}

/**
 * @test in case that a request is too long to be stored,
 */
BOOST_AUTO_TEST_CASE( read_buffer_overflow )
{
    const char header[] = "Accept-Encoding: gzip\r\n";

    std::vector<char>   input(begin(request_without_end_line), end(request_without_end_line));
    for ( unsigned i = 0; i != 10000; ++i )
        input.insert( input.end(), begin( header ), end(header) );

    typedef traits< server::test::response_factory, server::test::socket<std::vector<char>::const_iterator> > socket_t;
    boost::asio::io_service     queue;
    socket_t::connection_type   socket( queue, input.begin(), input.end() );
    traits<>                    traits;

    server::create_connection( socket, traits );

    queue.run();

    BOOST_REQUIRE_EQUAL( 1u, traits.requests().size() );
    BOOST_REQUIRE( traits.requests().front().get() );
    BOOST_CHECK_EQUAL( http::request_header::buffer_full, traits.requests().front()->state() );
}

/**
 * @brief sender closed connection, if request was ok, a response should have been send
 */
BOOST_AUTO_TEST_CASE( close_after_sender_closed )
{
    boost::asio::io_service     queue;
    traits<>::connection_type   socket(queue, begin(simple_get_11), end(simple_get_11), 0);
    traits<>                    trait;

    boost::weak_ptr<server::connection<traits<>, traits<>::connection_type> > connection(server::create_connection(socket, trait));
    BOOST_CHECK(!connection.expired());

    queue.run();

    trait.reset_responses();
    BOOST_CHECK_EQUAL("Hello", socket.output());
    BOOST_CHECK(connection.expired());
}

/**
 * @test client requests a close via connection header
 */
BOOST_AUTO_TEST_CASE( closed_by_connection_header )
{
    boost::asio::io_service     queue;
    traits<>::connection_type   socket(queue, begin(simple_get_11_with_close_header), end(simple_get_11_with_close_header), 0);
    traits<>                    trait;

    boost::weak_ptr<server::connection<traits<>, traits<>::connection_type> > connection(server::create_connection(socket, trait));
    BOOST_CHECK(!connection.expired());

    queue.run();

    trait.reset_responses();
    BOOST_CHECK_EQUAL("Hello", socket.output());

    // no outstanding reference to the connection object, so no read is pending on the connection to the client
    BOOST_CHECK(connection.expired());
}

/**
 * @test tests that a maximum idle time is not exceeded and the connection is closed 
 */
BOOST_AUTO_TEST_CASE( closed_when_idle_time_exceeded )
{
	using server::test::read;

    boost::asio::io_service     queue;
    read_plan                   reads;
    reads << read(begin(simple_get_11), end(simple_get_11))
          << delay(boost::posix_time::seconds(60))
          << read(begin(simple_get_11), end(simple_get_11))
          << read("");

    traits<>::connection_type   socket(queue, reads);
    traits<>                    trait;

    boost::weak_ptr<server::connection<traits<>, traits<>::connection_type> > connection(server::create_connection(socket, trait));
    BOOST_CHECK(!connection.expired());

    elapse_timer time;
    queue.run();

    // 30sec is the default idle time
    server::connection_config config;
    BOOST_CHECK_GE(time.elapsed(), config.keep_alive_timeout() - boost::posix_time::seconds(1));
    BOOST_CHECK_LE(time.elapsed(), config.keep_alive_timeout() + boost::posix_time::seconds(1));

    trait.reset_responses();
    BOOST_CHECK_EQUAL("Hello", socket.output());

    // no outstanding reference to the connection object, so no read is pending on the connection to the client
    BOOST_CHECK(connection.expired());
}

/**
 * @test the connection should be forced to be closed, when the connection is closed by the client
 */
BOOST_AUTO_TEST_CASE( closed_by_client_disconnected )
{
    boost::asio::io_service     queue;
    traits<>::connection_type   socket(queue, begin(simple_get_11), begin(simple_get_11) + (sizeof simple_get_11 / 2), 0);
    traits<>                    trait;

    boost::weak_ptr<server::connection<traits<>, traits<>::connection_type> > connection(server::create_connection(socket, trait));
    BOOST_CHECK(!connection.expired());

    queue.run();

    trait.reset_responses();
    BOOST_CHECK_EQUAL("", socket.output());
    BOOST_CHECK(connection.expired());
}

/**
 * @test handle timeout while writing to a client 
 */
BOOST_AUTO_TEST_CASE( timeout_while_writing_to_client )
{
	using server::test::read;
	using server::test::write;

	boost::asio::io_service     queue;

    read_plan                   reads;
    reads << read(begin(simple_get_11), end(simple_get_11))
          << delay(boost::posix_time::seconds(60))
          << read("");

    write_plan                  writes;
    writes << write(2) << delay(boost::posix_time::seconds(60));

    traits<>::connection_type   socket(queue, reads, writes);
    traits<>                    trait;

    boost::weak_ptr<server::connection<traits<>, traits<>::connection_type> > connection(server::create_connection(socket, trait));
    BOOST_CHECK(!connection.expired());

    elapse_timer time;
    queue.run();

    // 3sec is the default timeout
    server::connection_config config;
    BOOST_CHECK_GE(time.elapsed(), config.timeout() - boost::posix_time::seconds(1));
    BOOST_CHECK_LE(time.elapsed(), config.timeout() + boost::posix_time::seconds(1));

    trait.reset_responses();
    BOOST_CHECK_EQUAL("He", socket.output());

    // no outstanding reference to the connection object, so no read is pending on the connection to the client
    BOOST_CHECK(connection.expired());
}

/**
 * @test handle timeout while reading from a client
 *
 * The client starts sending a request header, but stops in the middle of the request to send further data.
 */
BOOST_AUTO_TEST_CASE( timeout_while_reading_from_client )
{
	using server::test::read;

	boost::asio::io_service     queue;
    read_plan                   reads;
    reads << read( begin( simple_get_11 ), begin( simple_get_11 ) + ( sizeof simple_get_11 / 2 ) )
          << delay( boost::posix_time::seconds( 60 ) )
          << read( begin( simple_get_11 ), end( simple_get_11 ) )
          << read( "" );

    traits<>::connection_type   socket(queue, reads);
    traits<>                    trait;

    boost::weak_ptr<server::connection<traits<>, traits<>::connection_type> > connection(server::create_connection(socket, trait));
    BOOST_CHECK(!connection.expired());

    elapse_timer time;
    queue.run();

    // 3sec is the default timeout
    server::connection_config config;
    BOOST_CHECK_GE(time.elapsed(), config.timeout() - boost::posix_time::seconds(1));
    BOOST_CHECK_LE(time.elapsed(), config.timeout() + boost::posix_time::seconds(1));

    trait.reset_responses();
    BOOST_CHECK_EQUAL("", socket.output());

    // no outstanding reference to the connection object, so no read is pending on the connection to the client

    BOOST_CHECK(connection.expired());
}

namespace {
    const boost::posix_time::time_duration response_delay = boost::posix_time::seconds( 35 );

    template < class Connection, class Trait >
    struct lasy_response : public server::async_response
    {
        lasy_response( const boost::shared_ptr< Connection >& c, boost::posix_time::time_duration d )
            : connection_( c )
            , timer_( c->socket().get_io_service() )
            , delay_( d )
        {}

        void end_delay( const boost::system::error_code& error )
        {
            BOOST_REQUIRE( !error );
            connection_->response_completed( *this );
        }

        void start()
        {
            timer_.expires_from_now( delay_ );
            timer_.async_wait( boost::bind( &lasy_response::end_delay, this, _1 ) );
        }

        boost::shared_ptr< Connection >         connection_;
        boost::asio::deadline_timer             timer_;
//        typename Trait::timeout_timer_type      timer_;
        const boost::posix_time::time_duration  delay_;
    };

    struct lasy_response_factory
    {
        lasy_response_factory() {}

        template < class T >
        explicit lasy_response_factory( const T& ) {}

        template < class Trait, class Connection >
        static boost::shared_ptr< server::async_response > create_response(
            const boost::shared_ptr< Connection >&                    connection,
            const boost::shared_ptr< const http::request_header >&    header,
                  Trait&                                              trait );
    };

    template < class Trait, class Connection >
    boost::shared_ptr< server::async_response > lasy_response_factory::create_response(
        const boost::shared_ptr< Connection >&                    connection,
        const boost::shared_ptr< const http::request_header >&    header,
              Trait&                                              trait )
    {

        BOOST_REQUIRE( response_delay > trait.keep_alive_timeout() );
        BOOST_REQUIRE( response_delay > trait.timeout() );

        return boost::shared_ptr< server::async_response >(
            new lasy_response< Connection, Trait >( connection, response_delay ) );
    }
} // namespace

/**
 * @test the read timeout should only be applied if there isn't any response outstanding
 *
 * First a response should block on a write for much longer then the keep alive timeout. This should not
 * lead to a timeout. After all data is written, the connection is idle and the keep alive timeout should apply
 */
BOOST_AUTO_TEST_CASE( no_readtimeout_when_responses_are_still_active )
{
    using server::test::read;
    using server::test::disconnect_read;

    boost::asio::io_service     queue;
    read_plan                   reads;
    reads << read( begin( simple_get_11 ), end( simple_get_11 ) )
          << delay( boost::posix_time::minutes( 60 ) )
          << disconnect_read();

    typedef traits< lasy_response_factory > trait_t;
    trait_t::connection_type   socket(queue, reads);
    trait_t                    trait;

    boost::weak_ptr< server::connection< trait_t, trait_t::connection_type > > connection(
        server::create_connection( socket, trait ) );
    BOOST_CHECK( !connection.expired() );

    elapse_timer time;
    queue.run();

    // 3sec is the default timeout; 35s the
    server::connection_config config;
    const boost::posix_time::time_duration expected_test_duration = config.keep_alive_timeout() + response_delay;
    BOOST_CHECK_GE(time.elapsed(), expected_test_duration - boost::posix_time::seconds(1));
    BOOST_CHECK_LE(time.elapsed(), expected_test_duration + boost::posix_time::seconds(1));

    trait.reset_responses();

    // no outstanding reference to the connection object, so no read is pending on the connection to the client
    BOOST_CHECK( connection.expired() );
}

/**
 * @test make sure no timeouts apply, when a client connects, close the connection and takes the response without
 *       any delay
 */
BOOST_AUTO_TEST_CASE( no_timeout_applies_when_client_doesnt_block )
{
    using server::test::read;
    using server::test::disconnect_read;

    boost::asio::io_service     queue;
    read_plan                   reads;
    reads << read( begin( simple_get_11 ), end( simple_get_11 ) )
          << disconnect_read();

    traits<>::connection_type   socket(queue, reads);
    traits<>                    trait;

    boost::weak_ptr< server::connection< traits<>, traits<>::connection_type > > connection(
        server::create_connection( socket, trait ) );
    BOOST_CHECK( !connection.expired() );

    elapse_timer time;
    queue.run();

    BOOST_CHECK_LE(time.elapsed(), boost::posix_time::seconds(1));

    trait.reset_responses();

    // no outstanding reference to the connection object, so no read is pending on the connection to the client
    BOOST_CHECK( connection.expired() );
}
