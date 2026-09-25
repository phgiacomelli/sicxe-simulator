#pragma once

#include "ExecutionEffect.hpp"
#include "MachineFault.hpp"
#include "Memory.hpp"

#include <cstdint>
#include <optional>

// PC que o efeito deixa no registrador: `nextPc`, ou o valor de uma escrita em PC.
std::uint32_t finalPc(const ExecutionEffect& effect);

// Valida o efeito inteiro sem alterar estado algum. Devolve a primeira falha encontrada.
std::optional<CycleFault> validateEffect(const ExecutionEffect& effect,
                                         std::uint32_t postFetchPc,
                                         const Memory& memory);
