// Copyright (c) Torrox GmbH & Co KG. All rights reserved.
// Please note that the content of this file is confidential or protected by law.
// Any unauthorised copying or unauthorised distribution of the information contained herein is prohibited.

#ifndef SIOUX_SRC_SERVER_TEST_IO_PLAN_H
#define SIOUX_SRC_SERVER_TEST_IO_PLAN_H

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <utility>
#include <vector>

namespace server
{

namespace test
{
    /**
     * @brief plan for simulated reads from a socket, with the data to be read and delays between them
     */
    class read_plan 
    {
    public:
        /** 
         * @brief an empty plan
         */
        read_plan();

        typedef std::pair<std::string, boost::posix_time::time_duration> item;

        /**
         * @brief returns the data for the next, read to perform. The time duration is the time until the read 
         * should be simulated.
         * @pre the plan must not be empty
         */
        item next_read();

        /**
         * @brief if the last item doesn't contain data to be read, it will be added to the last item.
         * If the last item contains already data, a new item is added
         */
        void add(const std::string&);

        /**
         * @brief adds a new item, with no data and the given delay
         */
        void delay(const boost::posix_time::time_duration& delay);

        /** 
         * @brief returns true, if the list is empty
         */
        bool empty() const;

    private:
        std::vector<item>                   steps_;
        std::vector<item>::size_type        next_;
    };

    struct read
    {
        explicit read(const std::string& s);

        template <class Iter>
        read(Iter begin, Iter end) : data(begin, end) {}

        std::string data;
    };

    struct delay
    {
        explicit delay(const boost::posix_time::time_duration&);
        boost::posix_time::time_duration delay_value;
    };

    /**
     * @brief adds a read to the read_plan
     * @relates read_plan
     */
    read_plan& operator<<(read_plan& plan, const read&);

    /**
     * @brief adds a delay to the read_plan
     * @relates read_plan
     */
    read_plan& operator<<(read_plan& plan, const delay&);

    /**
     * @brief plan, that describes, how large issued writes are performed and how much 
     * delay it to be issued between writes
     */
    class write_plan
    {
    public:
        /**
         * @brief an empty plan
         */
        write_plan();

        typedef std::pair<std::size_t, boost::posix_time::time_duration> item;

        /**
         * @brief returns the data for the next, write to perform. The time duration is the time until the write 
         * should be simulated.
         * @pre the plan must not be empty
         */
        item next_write();

        /**
         * @brief if the last item doesn't contain data to be write, it will be added to the last item.
         * If the last item contains already a size, a new item is added
         */
        void add(const std::size_t&);

        /**
         * @brief adds a new item, with no data and the given delay
         */
        void delay(const boost::posix_time::time_duration& delay);

        /** 
         * @brief returns true, if the list is empty
         */
        bool empty() const;

    private:
        std::vector<item>               plan_;
        std::vector<item>::size_type    next_;
    };

    struct write
    {
        explicit write(std::size_t s);
        std::size_t size;
    };

    /**
     * @brief adds a write to the write_plan
     * @relates write_plan
     */
    write_plan& operator<<(write_plan& plan, const write&);

    /**
     * @brief adds a delay to the write_plan
     * @relates write_plan
     */
    write_plan& operator<<(write_plan& plan, const delay&);

} // test
} // namespace server

#endif