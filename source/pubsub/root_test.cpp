// Copyright (c) Torrox GmbH & Co KG. All rights reserved.
// Please note that the content of this file is confidential or protected by law.
// Any unauthorised copying or unauthorised distribution of the information contained herein is prohibited.

#include "unittest++/unittest++.h"
#include "pubsub/node.h"
#include "pubsub/root.h"
#include "pubsub/test_helper.h"
#include "pubsub/configuration.h"
#include <boost/asio/io_service.hpp>

using namespace pubsub;

namespace {
    const node_name     random_node_name;
    const json::number  random_node_data(12);
    const node          random_node(node_version(), json::number(12));

    // for unknown reason, a single call to poll_one() will not consume one posted action    
    void poll_queue(boost::asio::io_service& io)
    {
        for ( unsigned int i = 10; i; --i)
            io.poll_one();
    }

    test::subscriber& test_user(const boost::shared_ptr<::pubsub::subscriber>& u)
    {
        assert(u.get());
        return dynamic_cast<test::subscriber&>(*u.get());
    }

    /// checks that just validation was requested for the given name, not authorization was requested for the 
    /// given user and no initlization was requested. The given user was not notified.
    bool only_validation_requested(const test::adapter& adapter, const node_name& name, const boost::shared_ptr<::pubsub::subscriber>& user)
    {
        return adapter.validation_requested(name)
            && !adapter.authorization_requested(user, name)
            && !adapter.initialization_requested(name)
            && test_user(user).not_on_udate_called();
    }

    bool only_authorization_requested(const test::adapter& adapter, const node_name& name, const boost::shared_ptr<::pubsub::subscriber>& user)
    {
        return !adapter.validation_requested(name)
            && adapter.authorization_requested(user, name)
            && !adapter.initialization_requested(name)
            && test_user(user).not_on_udate_called();
    }

    bool only_initialization_requested(const test::adapter& adapter, const node_name& name, const boost::shared_ptr<::pubsub::subscriber>& user)
    {
        return !adapter.validation_requested(name)
            && !adapter.authorization_requested(user, name)
            && adapter.initialization_requested(name)
            && test_user(user).not_on_udate_called();
    }
}

/**
 * @test check, that a new node is validated, authorized and initialized in 
 *        exactly this order and not before the first step returned success.
 */
TEST(subscribe_test)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(only_validation_requested(adapter, random_node_name, subscriber));

    adapter.answer_validation_request(random_node_name, true);
    poll_queue(queue);

    CHECK(only_authorization_requested(adapter, random_node_name, subscriber));

    adapter.answer_authorization_request(subscriber, random_node_name, true);
    poll_queue(queue);

    CHECK(only_initialization_requested(adapter, random_node_name, subscriber));

    adapter.answer_initialization_request(random_node_name, random_node_data);
    poll_queue(queue);

    CHECK(adapter.empty());
    CHECK(test_user(subscriber).on_udate_called(random_node_name, random_node_data));
    CHECK(test_user(subscriber).empty());
}

/**
 * @test same test, as above, but this time every request is answered synchronous
 */
TEST(synchronous_subscribe_test)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);

    adapter.answer_validation_request(random_node_name, true);
    adapter.answer_authorization_request(subscriber, random_node_name, true);
    adapter.answer_initialization_request(random_node_name, random_node_data);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(adapter.empty());
    CHECK(test_user(subscriber).on_udate_called(random_node_name, random_node_data));
    CHECK(test_user(subscriber).empty());
}

/**
 * @test check that a node that is configured to not require authorization,
 *       will not request authorization.
 */
TEST(subscribe_node_that_doesn_t_require_authorization)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configurator().authorization_not_required());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(only_validation_requested(adapter, random_node_name, subscriber));

    adapter.answer_validation_request(random_node_name, true);
    poll_queue(queue);

    CHECK(only_initialization_requested(adapter, random_node_name, subscriber));

    adapter.answer_initialization_request(random_node_name, random_node_data);
    poll_queue(queue);

    CHECK(adapter.empty());
    CHECK(test_user(subscriber).on_udate_called(random_node_name, random_node_data));
    CHECK(test_user(subscriber).empty());
}

/**
 * @test if validation fails, no more request should be made to the adapter and 
 *       failure should be reported to the subscriber and the adapter.
 */
TEST(subscribe_node_and_validation_failed)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(only_validation_requested(adapter, random_node_name, subscriber));

    adapter.answer_validation_request(random_node_name, false);
    poll_queue(queue);
    
    CHECK(adapter.invalid_node_subscription_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_invalid_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test if validation is skipped (not answered), no more request should be made to the adapter and 
 *       failure should be reported to the subscriber and the adapter.
 */
TEST(subscribe_node_and_validation_skipped)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(only_validation_requested(adapter, random_node_name, subscriber));

    adapter.skip_validation_request(random_node_name);
    poll_queue(queue);
    
    CHECK(adapter.invalid_node_subscription_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_invalid_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test synchronous skipping the validation (not storing the validation_call_back) should 
 *       result in a validation failure.
 */
TEST(subscribe_node_and_synchronous_validation_skipped)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);
    adapter.skip_validation_request(random_node_name);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(adapter.invalid_node_subscription_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_invalid_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test synchronous failing the validation should 
 *       result in a validation failure.
 */
TEST(subscribe_node_and_synchronous_validation_failed)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);
    adapter.answer_validation_request(random_node_name, false);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(adapter.invalid_node_subscription_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_invalid_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test after an authorization request failed asynchronous, no node initialization should have been requested 
 *       and failure must have been reported.
 */
TEST(subscribe_node_and_authorization_failed)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);
    adapter.answer_validation_request(random_node_name, true);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    adapter.answer_authorization_request(subscriber, random_node_name, false);
    poll_queue(queue);

    CHECK(adapter.unauthorized_subscription_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_unauthorized_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test
 */
TEST(subscribe_node_and_authorization_skipped)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);
    adapter.answer_validation_request(random_node_name, true);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    adapter.skip_authorization_request(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(adapter.unauthorized_subscription_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_unauthorized_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test 
 */
TEST(subscribe_node_and_synchronous_authorization_failed)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);
    adapter.answer_validation_request(random_node_name, true);
    adapter.answer_authorization_request(subscriber, random_node_name, false);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(adapter.unauthorized_subscription_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_unauthorized_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test
 */
TEST(subscribe_node_and_synchronous_authorization_skipped)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);
    adapter.answer_validation_request(random_node_name, true);
    adapter.skip_authorization_request(subscriber, random_node_name);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(adapter.unauthorized_subscription_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_unauthorized_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test
 */
TEST(subscribe_node_and_initialization_skipped)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);
    adapter.answer_validation_request(random_node_name, true);
    adapter.answer_authorization_request(subscriber, random_node_name, true);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    adapter.skip_initialization_request(random_node_name);
    poll_queue(queue);

    CHECK(adapter.initialization_failed_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_failed_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}

/**
 * @test
 */
TEST(subscribe_node_and_synchronous_initialization_skipped)
{
    boost::asio::io_service                 queue;
    test::adapter                           adapter;
    pubsub::root                            root(queue, adapter, configuration());

    boost::shared_ptr<::pubsub::subscriber> subscriber(new test::subscriber);
    adapter.answer_validation_request(random_node_name, true);
    adapter.answer_authorization_request(subscriber, random_node_name, true);
    adapter.skip_initialization_request(random_node_name);

    root.subscribe(subscriber, random_node_name);
    poll_queue(queue);

    CHECK(adapter.initialization_failed_reported(random_node_name, subscriber));
    CHECK(test_user(subscriber).on_failed_node_subscription_called(random_node_name));
    CHECK(test_user(subscriber).empty());
    CHECK(adapter.empty());
}
