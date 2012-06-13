/*
 * Copyright (C) 2012 Sony Mobile Communications AB.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SENSOR_UTIL_LIST_H_
#define SENSOR_UTIL_LIST_H_

struct list_node {
	struct list_node *n;
	struct list_node *p;
};

static inline void node_init(struct list_node *node)
{
	node->n = node->p = node;
}

static inline void node_add(struct list_node *head, struct list_node *node)
{
	node->n = head->n;
	node->p = head;
	head->n = node;
	node->n->p = node;
}

static inline void node_del(struct list_node *node)
{
	node->p->n = node->n;
	node->n->p = node->p;
}

static inline void node_del_init(struct list_node *node)
{
	node_del(node);
	node_init(node);
}
#endif
