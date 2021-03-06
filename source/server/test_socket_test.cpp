// Copyright (c) Torrox GmbH & Co KG. All rights reserved.
// Please note that the content of this file is confidential or protected by law.
// Any unauthorised copying or unauthorised distribution of the information contained herein is prohibited.

#include <boost/test/unit_test.hpp>
#include "server/test_socket.h"
#include "server/test_tools.h"
#include "server/timeout.h"
#include "http/test_request_texts.h"
#include "tools/io_service.h"
#include "tools/asstring.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>

using namespace server::test;
using namespace http::test;

BOOST_AUTO_TEST_CASE(read_timeout_test)
{
    io_completed result;

    boost::asio::io_service queue;
    server::test::socket<const char*> sock(queue, begin(simple_get_11), end(simple_get_11), 5, 
        boost::posix_time::seconds(1), boost::posix_time::time_duration());
    char b[sizeof simple_get_11];

    const boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();

    sock.async_read_some(boost::asio::buffer(b), result);
    tools::run(queue);

    BOOST_CHECK_GE(boost::posix_time::microsec_clock::universal_time() - t1, boost::posix_time::seconds(1) - boost::posix_time::millisec(100));
    BOOST_CHECK_LE(boost::posix_time::microsec_clock::universal_time() - t1, boost::posix_time::seconds(1) + boost::posix_time::millisec(100));
    BOOST_CHECK_EQUAL(5u, result.bytes_transferred);
    BOOST_CHECK(!result.error);
}

BOOST_AUTO_TEST_CASE(write_timeout_test)
{
    io_completed result;

    boost::asio::io_service queue;
    server::test::socket<const char*> sock(queue, begin(simple_get_11), end(simple_get_11), 5, 
        boost::posix_time::time_duration(), boost::posix_time::seconds(1));

    const boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();

    sock.async_write_some(boost::asio::buffer(simple_get_11, 5), result);
    tools::run(queue);

    BOOST_CHECK_GE(boost::posix_time::microsec_clock::universal_time() - t1, boost::posix_time::seconds(1) - boost::posix_time::millisec(100));
    BOOST_CHECK_LE(boost::posix_time::microsec_clock::universal_time() - t1, boost::posix_time::seconds(1) + boost::posix_time::millisec(100));
    BOOST_CHECK_EQUAL(5u, result.bytes_transferred);
    BOOST_CHECK(!result.error);
}

BOOST_AUTO_TEST_CASE(chancel_read_write)
{
    io_completed result_read;
    io_completed result_write;

    boost::asio::io_service queue;
    server::test::socket<const char*> sock(queue, begin(simple_get_11), end(simple_get_11), 5, boost::posix_time::seconds(1));
    char b[sizeof simple_get_11];

    sock.async_read_some(boost::asio::buffer(b), result_read);
    sock.async_write_some(boost::asio::buffer(simple_get_11, 5), result_write);
    sock.close();

    tools::run(queue);

    BOOST_CHECK_EQUAL(0u, result_read.bytes_transferred);
    BOOST_CHECK_EQUAL(0u, result_write.bytes_transferred);
    BOOST_CHECK_EQUAL(make_error_code(boost::asio::error::operation_aborted), result_read.error);
    BOOST_CHECK_EQUAL(make_error_code(boost::asio::error::operation_aborted), result_write.error);
}

/**
 * @test reading with timeout, using the async_read_some_with_to() function
 */
BOOST_AUTO_TEST_CASE(async_read_some_with_to_test)
{
    io_completed result;

    boost::asio::io_service     queue;
    boost::asio::deadline_timer timer(queue);
    server::test::socket<const char*> sock(queue, begin(simple_get_11), end(simple_get_11), 5, boost::posix_time::seconds(1));
    char b[sizeof simple_get_11];

    server::async_read_some_with_to(sock, boost::asio::buffer(b), result, timer, boost::posix_time::milliseconds(20));

    tools::run(queue);

    BOOST_CHECK_EQUAL(0u, result.bytes_transferred);
    BOOST_CHECK_EQUAL(result.error, make_error_code(server::time_out));
}

/**
 * @test tests the use of testplans
 *
 * Executing 2 reads and 2 writes and then comparing the results with the expected results and timing
 */
BOOST_AUTO_TEST_CASE(use_test_plan)
{
	using server::test::read;
	using server::test::write;

	read_plan reads;
    reads << read("hallo Welt") << delay(boost::posix_time::millisec(1000)) << read("");

    write_plan writes;
    writes << delay(boost::posix_time::millisec(2000)) << write(20) << write(5);

    BOOST_REQUIRE( !reads.empty() );
    BOOST_REQUIRE( !writes.empty() );

    boost::asio::io_service     queue;
    server::test::socket<const char*> sock(queue, reads, writes);

    io_completed first_read;
    io_completed second_read;
    io_completed first_write;
    io_completed second_write;

    const boost::posix_time::ptime start_time = boost::posix_time::microsec_clock::universal_time();

    char read_buffer[100] = {0};

    sock.async_read_some(boost::asio::buffer(read_buffer), first_read);
    sock.async_read_some(
        boost::asio::buffer(&read_buffer[first_read.bytes_transferred], 
            sizeof read_buffer - first_read.bytes_transferred), 
        second_read);

    sock.async_write_some(boost::asio::buffer(read_buffer), first_write);
    tools::run(queue);

    sock.async_write_some(boost::asio::buffer(read_buffer), second_write);
    tools::run(queue);

    const boost::posix_time::ptime          now = boost::posix_time::microsec_clock::universal_time();
    const boost::posix_time::time_duration  tolerance = boost::posix_time::millisec( 100 );

    BOOST_CHECK_GE(first_read.when, start_time - tolerance);
    BOOST_CHECK_LE(first_read.when, start_time + tolerance);
    BOOST_CHECK_EQUAL(10u, first_read.bytes_transferred);
    BOOST_CHECK_EQUAL(std::string("hallo Welt"), std::string(&read_buffer[0], &read_buffer[10]));
    BOOST_CHECK(!first_read.error);

    BOOST_CHECK_GE(second_read.when, start_time + boost::posix_time::millisec(1000) - tolerance);
    BOOST_CHECK_LE(second_read.when, start_time + boost::posix_time::millisec(1000) + tolerance);
    BOOST_CHECK_EQUAL(0u, second_read.bytes_transferred);
    BOOST_CHECK(!second_read.error);

    BOOST_CHECK_GE(first_write.when, start_time + boost::posix_time::millisec(2000) - tolerance);
    BOOST_CHECK_LE(first_write.when, start_time + boost::posix_time::millisec(2000) + tolerance);
    BOOST_CHECK_EQUAL(20u, first_write.bytes_transferred);
    BOOST_CHECK(!first_write.error);

    BOOST_CHECK_GE(second_write.when, start_time + boost::posix_time::millisec(2000) - tolerance);
    BOOST_CHECK_LE(second_write.when, start_time + boost::posix_time::millisec(2000) + tolerance);
    BOOST_CHECK_EQUAL(5u, second_write.bytes_transferred);
    BOOST_CHECK(!second_write.error);
}

namespace {
    struct read_handler
    {
        char*                               buffer;
        server::test::socket<const char*>&  socket;

        void operator()(const boost::system::error_code& error, std::size_t bytes_transferred)
        {
            if ( !error )
            {
                buffer += bytes_transferred;

                if ( bytes_transferred != 0 )
                {
                    socket.async_read_some(boost::asio::buffer(buffer, sizeof simple_get_11_with_close_header), *this);
                }
            }
        }
    };
}

BOOST_AUTO_TEST_CASE(first_read_followed_by_delay_and_second_read)
{
	using server::test::read;

	boost::asio::io_service     queue;
    read_plan                   reads;
    reads << read(begin(simple_get_11_with_close_header), end(simple_get_11_with_close_header))
          << delay(boost::posix_time::seconds(1))
          << read(begin(simple_get_11_with_close_header), end(simple_get_11_with_close_header))
          << read("");

    char buffer[10 * sizeof simple_get_11_with_close_header];

    server::test::socket<const char*> socket(queue, reads);
    read_handler h = {buffer, socket};

    elapse_timer time;

    socket.async_read_some(boost::asio::buffer(buffer), h);
    tools::run(queue);

    BOOST_CHECK_GE(time.elapsed(), boost::posix_time::seconds(1) - boost::posix_time::millisec(100));
    BOOST_CHECK_LE(time.elapsed(), boost::posix_time::seconds(1) + boost::posix_time::millisec(100));
}

/**
 * @test a configured read error must occure after a defined size is read from the socket
 */
BOOST_AUTO_TEST_CASE( simulate_read_error )
{
	boost::asio::io_service		queue;
    server::test::socket<const char*> sock(queue, begin(simple_get_11), end(simple_get_11),
    		make_error_code(boost::asio::error::operation_aborted), 5u,
    		make_error_code(boost::asio::error::operation_aborted), 0 );

    io_completed first_read;
    io_completed second_read;

    char read_buffer[10] = {0};

    sock.async_read_some(boost::asio::buffer(read_buffer), first_read);
    sock.async_read_some(boost::asio::buffer(read_buffer), second_read);

    tools::run(queue);
    BOOST_CHECK( !first_read.error );
    BOOST_CHECK_EQUAL( 5u, first_read.bytes_transferred );
    BOOST_CHECK_EQUAL( make_error_code(boost::asio::error::operation_aborted), second_read.error );
    BOOST_CHECK_EQUAL( 0, second_read.bytes_transferred );
}

/**
 * @test make sure, that remote_endpoint() returns the expected result
 */
BOOST_AUTO_TEST_CASE( remote_endpoint_returns_the_expected_value )
{
	BOOST_CHECK_EQUAL( "192.168.210.1:9999",
			tools::as_string( server::test::socket<const char*>().remote_endpoint() ) );
}

static void increment( int& i )
{
    ++i;
}

/**
 * @test make sure, that read_plan::execute() will be called
 */
BOOST_AUTO_TEST_CASE( read_plan_execute_test )
{
    namespace pt = boost::posix_time;
    using server::test::read;

    int i = 0;
    read_plan   plan;
    plan << boost::bind( &increment, boost::ref( i ) ) << read( "a" )
         << delay( pt::millisec(100) ) << read( "b" )
         << delay( pt::millisec(200) ) << boost::bind( &increment, boost::ref( i ) ) << read( "c" )
         << boost::bind( &increment, boost::ref( i ) )
         << read( "d" );

    BOOST_REQUIRE_EQUAL( i, 0 );
    read_plan::item item = plan.next_read();
    BOOST_CHECK_EQUAL( i, 1 );
    BOOST_CHECK( item == std::make_pair( std::string( "a" ), pt::time_duration() ) );

    item = plan.next_read();
    BOOST_CHECK_EQUAL( i, 1 );
    BOOST_CHECK( item == std::make_pair( std::string( "b" ), pt::time_duration( pt::millisec( 100 ) ) ) );

    item = plan.next_read();
    BOOST_CHECK_EQUAL( i, 1 );
    BOOST_CHECK( item == std::make_pair( std::string( "" ), pt::time_duration( pt::millisec( 200 ) ) ) );

    item = plan.next_read();
    BOOST_CHECK_EQUAL( i, 2 );
    BOOST_CHECK( item == std::make_pair( std::string( "c" ), pt::time_duration() ) );

    item = plan.next_read();
    BOOST_CHECK_EQUAL( i, 3 );
    BOOST_CHECK( item == std::make_pair( std::string( "d" ), pt::time_duration() ) );
}

BOOST_AUTO_TEST_CASE( write_callback_is_called )
{
    using namespace boost::lambda;

    boost::asio::io_service         queue;
    server::test::socket<>          socket( queue, read_plan() );
    boost::asio::const_buffer       buffer;
    io_completed                    io_result;

    socket.write_callback( var( buffer ) = boost::lambda::_1 );
    BOOST_REQUIRE_EQUAL( buffer_size( buffer ), 0 );

    socket.async_write_some( boost::asio::buffer( "Hallo" ), io_result );
    tools::run( queue );

    BOOST_CHECK( !io_result.error );
    BOOST_CHECK_EQUAL( io_result.bytes_transferred, 6u );

    BOOST_CHECK_EQUAL( buffer_size( buffer ), 6u );
    BOOST_CHECK_EQUAL( std::string( "Hallo" ), boost::asio::buffer_cast< const char* >( buffer ) );

    socket.async_write_some( boost::asio::buffer( "Welt" ), io_result );
    tools::run( queue );

    BOOST_CHECK_EQUAL( buffer_size( buffer ), 5u );
    BOOST_CHECK_EQUAL( std::string( "Welt" ), boost::asio::buffer_cast< const char* >( buffer ) );
}

