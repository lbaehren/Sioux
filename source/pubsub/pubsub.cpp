// Copyright (c) Torrox GmbH & Co KG. All rights reserved.
// Please note that the content of this file is confidential or protected by law.
// Any unauthorised copying or unauthorised distribution of the information contained herein is prohibited.

#include "pubsub/pubsub.h"

namespace pubsub
{
    // class adapter
    void adapter::invalid_node_subscription(const node_name&, const boost::shared_ptr<subscriber>&)
    {
    }

    void adapter::unauthorized_subscription(const node_name&, const boost::shared_ptr<subscriber>&)
    {
    }

    void adapter::initialization_failed(const node_name&, const boost::shared_ptr<subscriber>&)
    {
    }

} // namespace pubsub




