// Copyright 2010-2022 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/routing/simple_graph.h"

namespace operations_research {

Edge::Edge(const Arc& arc) : tail_(arc.tail()), head_(arc.head()) {}
Arc::Arc(const Edge& edge) : tail_(edge.tail()), head_(edge.head()) {}

}  // namespace operations_research
