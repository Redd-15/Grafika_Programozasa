// Model: turns any JSON document into dotted "path -> value" signals.
// Arrays of objects use the element's "name" (or hex "id") as the path segment;
// {"name": n, "value": v} pairs become a single leaf "<parent>.n". If the text is not
// one JSON document it is read as newline-delimited JSON.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "model/sources/signal_mapping.h"

void FlattenJson(const std::string& text, std::vector<std::pair<std::string, SignalValue>>& out, bool* ok);
