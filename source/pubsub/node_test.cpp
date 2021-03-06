// Copyright (c) Torrox GmbH & Co KG. All rights reserved.
// Please note that the content of this file is confidential or protected by law.
// Any unauthorised copying or unauthorised distribution of the information contained herein is prohibited.

#include <boost/test/unit_test.hpp>
#include <utility>
#include "pubsub/node.h"
#include "pubsub/key.h"
#include "json/json.h"
#include "json/delta.h"

/**
 * @test node_name::empty() test
 */
BOOST_AUTO_TEST_CASE( node_name_empty_test )
{
    const pubsub::node_name empty_name;
    BOOST_CHECK( empty_name.empty() );

    pubsub::node_name other_name;
    BOOST_CHECK( other_name.empty() );
    other_name.add( pubsub::key( pubsub::key_domain( "key" ), "value" ) );

    BOOST_CHECK( !other_name.empty() );

    other_name = empty_name;
    BOOST_CHECK( other_name.empty() );
}

/**
 * @test test the constructor
 */
BOOST_AUTO_TEST_CASE(node_ctor)
{
    const pubsub::node_version  current_version;
    const json::value           data = json::parse("\"Hallo\"");

    const pubsub::node          node(current_version, data);

    BOOST_CHECK(node.current_version() == current_version);
    BOOST_CHECK(node.oldest_version() == current_version);
    BOOST_CHECK(node.data() == data);
    BOOST_CHECK(node.get_update_from(current_version) == std::make_pair(false, data));
    BOOST_CHECK(node.get_update_from(current_version-5u) == std::make_pair(false, data));
}

namespace {
    const json::value  version1 = json::parse("[1,2,3,4,5,6,7,8,10]");
    const json::value  version2 = json::parse("[1,3,4,5,6,7,8,10]");
    const json::value  version3 = json::parse("[]");
    const json::value  version4 = json::parse("[1]");

    const json::array  updata_to2 = json::delta(version1, version2, 100000000u).second.upcast<json::array>();
    const json::array  updata_to3 = json::delta(version2, version3, 100000000u).second.upcast<json::array>();
    const json::array  updata_to4 = json::delta(version3, version4, 100000000u).second.upcast<json::array>();

    bool check_update(const json::value& from, const json::value& to, const std::pair<bool, json::value>& update)
    {
        if ( !update.first ) 
            return false;

        json::value copy_from(json::parse(from.to_json()));
        const json::array update_list = update.second.upcast<json::array>();

        for ( std::size_t i = 0; i != update_list.length(); ++i )
            copy_from = json::update(copy_from, update_list.at(i));

        return to == copy_from;
    }
}

BOOST_AUTO_TEST_CASE(node_update)
{
    const pubsub::node_version  first_version;
    pubsub::node_version        current_version(first_version);
    pubsub::node                node(current_version, version1);

    BOOST_CHECK_EQUAL(version1, node.data());
    BOOST_CHECK_EQUAL(current_version, node.current_version());
    BOOST_CHECK_EQUAL(first_version, node.oldest_version());
    
    ++current_version;
    node.update(version2, 1000u);
                                                                                                                      
    BOOST_CHECK_EQUAL(version2, node.data());
    BOOST_CHECK_EQUAL(current_version, node.current_version());
    BOOST_CHECK_EQUAL(first_version, node.oldest_version());  
    BOOST_CHECK(check_update(version1, version2, node.get_update_from(first_version)));

    ++current_version;
    node.update(version3, 1000000u);

    BOOST_CHECK_EQUAL(version3, node.data());
    BOOST_CHECK_EQUAL(current_version, node.current_version());
    BOOST_CHECK_EQUAL(first_version, node.oldest_version());  
    BOOST_CHECK(check_update(version1, version3, node.get_update_from(first_version)));

    ++current_version;
    node.update(version4, 1000000u);

    BOOST_CHECK_EQUAL(version4, node.data());
    BOOST_CHECK_EQUAL(current_version, node.current_version());
    BOOST_CHECK_EQUAL(first_version, node.oldest_version());  
    BOOST_CHECK(check_update(version1, version4, node.get_update_from(first_version)));

    BOOST_CHECK(check_update(version3, version4, node.get_update_from(current_version-1)));
    BOOST_CHECK(check_update(version2, version4, node.get_update_from(current_version-2)));
    BOOST_CHECK(check_update(version1, version4, node.get_update_from(current_version-3)));
}

BOOST_AUTO_TEST_CASE(node_update_limit)
{
    pubsub::node_version        current_version;
    pubsub::node                node(current_version, version1);

    for ( unsigned i = 0 ; i != 20; ++i )
    {
        const json::value new_value = (i % 2 == 0) ? version2 : version1;
        const json::value old_value = (i % 2 == 1) ? version2 : version1;

        node.update(new_value, 50);
        ++current_version;

        BOOST_CHECK_EQUAL(new_value, node.data());
        BOOST_CHECK_EQUAL(current_version, node.current_version());
        BOOST_CHECK_EQUAL(current_version-1, node.oldest_version());
        BOOST_CHECK(check_update(old_value, new_value, node.get_update_from(current_version-1)));
    }

    for ( unsigned i = 0 ; i != 20; ++i )
    {
        const json::value new_value = (i % 2 == 0) ? version2 : version1;
        const json::value old_value = (i % 2 == 1) ? version2 : version1;

        node.update(new_value, 90);
        ++current_version;

        BOOST_CHECK_EQUAL(new_value, node.data());
        BOOST_CHECK_EQUAL(current_version, node.current_version());
        BOOST_CHECK_EQUAL(current_version-2, node.oldest_version());
        BOOST_CHECK(check_update(new_value, new_value, node.get_update_from(current_version-2)));
    }
}

BOOST_AUTO_TEST_CASE(node_equal_data)
{
    const pubsub::node_version  current_version;
    pubsub::node                node(current_version, version1);

    BOOST_CHECK_EQUAL(version1, node.data());
    BOOST_CHECK_EQUAL(current_version, node.current_version());
    BOOST_CHECK_EQUAL(current_version, node.oldest_version());

    node.update(version1, 0);

    BOOST_CHECK_EQUAL(version1, node.data());
    BOOST_CHECK_EQUAL(current_version, node.current_version());
    BOOST_CHECK_EQUAL(current_version, node.oldest_version());

    node.update(version1, 100000u);

    BOOST_CHECK_EQUAL(version1, node.data());
    BOOST_CHECK_EQUAL(current_version, node.current_version());
    BOOST_CHECK_EQUAL(current_version, node.oldest_version());
}

BOOST_AUTO_TEST_CASE( node_add_keys )
{
	pubsub::node_name name1;

	const pubsub::key k1( pubsub::key_domain( "p1" ), "v1" );
	const pubsub::key k2( pubsub::key_domain( "p2" ), "v2" );

	BOOST_CHECK_EQUAL( &name1, &name1.add( k1 ) );
	BOOST_CHECK( std::make_pair( true, k1 ) == name1.find_key( pubsub::key_domain( "p1" ) ) );

	BOOST_CHECK_EQUAL( &name1, &name1.add( k2 ) );
	BOOST_CHECK( std::make_pair( true, k2 ) == name1.find_key( pubsub::key_domain( "p2" ) ) );

	pubsub::node_name name2;

	BOOST_CHECK_EQUAL( &name2, &name2.add( k2 ) );
	BOOST_CHECK( std::make_pair( true, k2 ) == name2.find_key( pubsub::key_domain( "p2" ) ) );

	BOOST_CHECK_EQUAL( &name2, &name2.add( k1 ) );
	BOOST_CHECK( std::make_pair( true, k1 ) == name2.find_key( pubsub::key_domain( "p1" ) ) );

	BOOST_CHECK_EQUAL( name1, name2 );
}
