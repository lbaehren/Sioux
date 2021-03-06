// Copyright (c) Torrox GmbH & Co KG. All rights reserved.
// Please note that the content of this file is confidential or protected by law.
// Any unauthorised copying or unauthorised distribution of the information contained herein is prohibited.

#ifndef SIOUX_SERVER_TEST_TOOLS_H
#define SIOUX_SERVER_TEST_TOOLS_H

#include <vector>
#include <iosfwd>
#include <boost/asio/io_service.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

namespace server
{
    namespace test {

        /**
         * @brief returns a sequence of pseudo-random bytes with the given length
         */
        std::vector<char> random_body(boost::minstd_rand& random, std::size_t size);

        /**
         * @brief returns the given body chunked encoded with the chunks being randomly sized
         */
        std::vector<char> random_chunk(boost::minstd_rand& random, const std::vector<char>& original, std::size_t max_chunk_size);

        /**
         * @brief compares two buffer and produce a "helpful" message, if a difference is found
         */
        bool compare_buffers(const std::vector<char>& org, const std::vector<char>& comp, std::ostream& report);

        /**
         * @brief waits for the given period and returns then
         */
        void wait(const boost::posix_time::time_duration& period);

        /**
         * @brief times the time since construction
         */
        class elapse_timer
        {
        public:
            elapse_timer();

            boost::posix_time::time_duration elapsed() const;
        private:
            boost::posix_time::ptime    start_;
        };

        /**
         * @brief provide a call back for io functions and record the parameters and the time of the call.
         *
         * to provide reference semantic, each object points to an othere copy of an original created with
         * the default c'tor.
         */
        struct io_completed
        {
            io_completed();
            io_completed(const io_completed& org);
            ~io_completed();
            io_completed& operator=(const io_completed& rhs);

            void swap(io_completed& other);

            void operator()(const boost::system::error_code& e, std::size_t b);

            boost::system::error_code   error;
            size_t                      bytes_transferred;
            boost::posix_time::ptime    when;

        private:
            mutable io_completed*       next_;
        };

    } // namespace test

} // namespace server

#endif // include guard

