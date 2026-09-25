#pragma once

#include "ExecutionEffect.hpp"
#include "MachineFault.hpp"
#include "Memory.hpp"

#include <cstdint>
#include <optional>

// Valida o efeito inteiro sem alterar estado algum. Devolve a primeira falha encontrada.
std::optional<CycleFault> validateEffect(const ExecutionEffect& effect, const Memory& memory);
