// Copyright (c) 2026 Peter Martienssen
// SPDX-License-Identifier: MIT

#pragma once

#include <any>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/Logger.hpp"

/**
 * @brief Type-erased, zero-copy key-value store for per-frame pipeline data.
 *
 * FrameContext is the central per-frame data carrier passed through the runtime.
 * It stores arbitrary typed values (images, metadata, control parameters,
 * diagnostics, and other node outputs) under string keys and keeps them grouped
 * by scope. Nodes use it to exchange runtime data without needing direct
 * references to each other.
 *
 * The context is intentionally simple: nodes can write values into the current
 * write scope, explicitly target another scope, and later read values either by
 * fully qualified key or by unqualified lookup across the current scope order.
 * This makes the class suitable both for linear pipelines and for merged graph
 * execution where multiple upstream nodes contribute data to the same frame.
 *
 * ### Zero-copy semantics
 * Values are stored as `std::shared_ptr<std::any>` so that they can be shared
 * between multiple nodes (e.g. in a branch) and across merge points without any
 * deep copy. A value is only physically duplicated when a node explicitly
 * overwrites an existing key via @ref set.
 *
 * Large node outputs (e.g. @ref ImageBuffer instances) should always be written
 * using move semantics:
 * @code
 * ImageBuffer img = ...;
 * context.set("image", std::move(img));
 * @endcode
 *
 * ### Scoped namespace behaviour
 * Keys are stored in explicit scopes. Unqualified writes use the currently active
 * write scope, while qualified writes such as `cam0.xpos` target the specified
 * scope directly. Reads resolve in this order:
 * - exact qualified key match (`scope.key`)
 * - unqualified key in the stored scope order, searched backwards so the most
 *   recently written scope wins
 *
 * This behavior allows downstream nodes to access the latest available value
 * without having to know which upstream node wrote it, while still supporting
 * exact scope-based access when a node depends on a specific producer.
 *
 * ### Template usage
 * @code
 * // Producer node:
 * context.set("image", std::move(imageBuffer));
 *
 * // Consumer node:
 * const ImageBuffer* img = context.get<ImageBuffer>("image");
 * if (img != nullptr) { ... }
 * @endcode
 *
 * @see ImageBuffer
 * @see IPipeline
 */
class FrameContext
{
public:
    struct Entry
    {
        std::shared_ptr<std::any> value; ///< Shared pointer to the type-erased value.
    };

    /**
     * @brief Sets the default scope for unqualified writes.
     *
     * Any later call to @ref set(name, value) without an explicit scope writes
     * into this scope until it is changed again. Schedulers use this to map the
     * current node's outputs to its own namespace.
     *
     * @param scope Scope prefix used for unqualified writes.
     */
    void setWriteScope(const std::string& scope);

    /**
     * @brief Replaces the execution order used for unqualified reads.
     *
     * The order is stored as a scope stack. When a node calls @ref get(name)
     * without an explicit scope, the context searches this list backwards so the
     * most recently published matching value wins.
     *
     * @param scopes Scope names in execution order.
     */
    void setReadScopes(const std::vector<std::string>& scopes);

    /**
     * @brief Returns the scope currently used for unqualified writes.
     * @return The active write scope name, or an empty string if none is set.
     */
    const std::string& writeScope() const;

    /**
     * @brief Returns the current unqualified read order.
     * @return Scope names in execution order.
     */
    const std::vector<std::string>& readScopes() const;

    /**
     * @brief Stores a value under the given key in the current write scope.
     *
     * This overload is used for the common case where a node writes its output
     * into its own namespace. If the name is already qualified (for example
     * `cam0.xpos`), the explicit scope is honored instead of the active write
     * scope.
     *
     * The value is wrapped in a `std::shared_ptr<std::any>` so that downstream
     * nodes can reuse the same payload without copying it.
     *
     * @tparam T    Type of the value to store.
     * @param name  Logical key or fully qualified key.
     * @param value Value to store; use `std::move` for large objects.
     */
    template <typename T> void set(const std::string& name, T&& value)
    {
        const QualifiedName qualified = splitQualifiedName(name);
        const std::string scope = qualified.scope.empty() ? m_writeScope : qualified.scope;
        const std::string key = qualified.key;
        set(scope, key, std::forward<T>(value));
    }

    /**
     * @brief Stores a value under an explicit scope and key.
     *
     * This overload bypasses the active write scope and is typically used when
     * a node wants to publish data to another node's namespace or when a source
     * node prepares multiple scoped outputs in one frame.
     *
     * @tparam T     Type of the value to store.
     * @param scope  Target scope (for example `cam0`).
     * @param name   Logical key inside the scope (for example `image`).
     * @param value  Value to store.
     */
    template <typename T> void set(const std::string& scope, const std::string& name, T&& value)
    {
        touchScope(scope);
        m_scopes[scope][name] = {std::make_shared<std::any>(std::forward<T>(value))};
    }

    /**
     * @brief Returns a mutable pointer to a required value in the current scope order.
     *
     * The lookup first checks whether @p name is explicitly qualified. If not,
     * the current read scope order is searched backwards until a matching key is
     * found. Missing keys are logged because this overload expresses a required
     * dependency.
     *
     * @tparam T    Expected type of the stored value.
     * @param name  Key to look up.
     * @return Pointer to the value, or @c nullptr on missing entry or type mismatch.
     */
    template <typename T> T* get(const std::string& name)
    {
        Entry* entry = resolveEntry(name);
        if (entry == nullptr || !entry->value) {
            logResolveFailure(name, m_writeScope);
            return nullptr;
        }

        T* value = std::any_cast<T>(entry->value.get());
        if (value == nullptr) {
            logTypeMismatch(name, entry->value->type().name(), typeid(T).name(), m_writeScope);
        }
        return value;
    }

    /**
     * @brief Returns a mutable pointer to a required value from an explicit scope.
     *
     * This overload bypasses the scope-order search and resolves only the exact
     * scope/key pair that was requested. It is useful for nodes that depend on a
     * concrete producer namespace, such as compositor layers.
     *
     * @tparam T     Expected value type.
     * @param scope  Scope name.
     * @param name   Logical key name.
     * @return Pointer to value, or @c nullptr if missing/type mismatch.
     */
    template <typename T> T* get(const std::string& scope, const std::string& name)
    {
        Entry* entry = resolveScopedEntry(scope, name);
        if (entry == nullptr || !entry->value) {
            logResolveFailure(qualifiedKey(scope, name), m_writeScope);
            return nullptr;
        }

        T* value = std::any_cast<T>(entry->value.get());
        if (value == nullptr) {
            logTypeMismatch(qualifiedKey(scope, name), entry->value->type().name(), typeid(T).name(), m_writeScope);
        }
        return value;
    }

    /**
     * @brief Returns a read-only pointer to a required value in the current scope order.
     *
     * This const overload follows the same resolution rules as the mutable
     * version of @ref get(name): explicit scope first, then reverse execution
     * order for unqualified reads.
     *
     * @tparam T    Expected type of the stored value.
     * @param name  Key to look up.
     * @return Const pointer to the value, or @c nullptr on missing entry or type mismatch.
     */
    template <typename T> const T* get(const std::string& name) const
    {
        const Entry* entry = resolveEntry(name);
        if (entry == nullptr || !entry->value) {
            logResolveFailure(name, m_writeScope);
            return nullptr;
        }

        const T* value = std::any_cast<T>(entry->value.get());
        if (value == nullptr) {
            logTypeMismatch(name, entry->value->type().name(), typeid(T).name(), m_writeScope);
        }
        return value;
    }

    /**
     * @brief Returns a read-only pointer to a required value from an explicit scope.
     *
     * This overload is the const counterpart to the scoped mutable lookup and is
     * intended for consumers that need read-only access to a specific producer's
     * data.
     *
     * @tparam T     Expected value type.
     * @param scope  Scope name.
     * @param name   Logical key name.
     * @return Const pointer to value, or @c nullptr if missing/type mismatch.
     */
    template <typename T> const T* get(const std::string& scope, const std::string& name) const
    {
        const Entry* entry = resolveScopedEntry(scope, name);
        if (entry == nullptr || !entry->value) {
            logResolveFailure(qualifiedKey(scope, name), m_writeScope);
            return nullptr;
        }

        const T* value = std::any_cast<T>(entry->value.get());
        if (value == nullptr) {
            logTypeMismatch(qualifiedKey(scope, name), entry->value->type().name(), typeid(T).name(), m_writeScope);
        }
        return value;
    }

    /**
     * @brief Returns the stored value or a fallback when the key is unavailable.
     *
     * This overload is for optional dependencies. Missing keys are not logged,
     * which makes it suitable for tunables such as positions or z-order values.
     * Type mismatches are still reported because they usually indicate a coding
     * or configuration error.
     *
     * @tparam T            Expected value type.
     * @param name          Key to look up.
     * @param defaultValue  Fallback value.
     * @return Stored value or @p defaultValue.
     */
    template <typename T> T get(const std::string& name, const T& defaultValue) const
    {
        const Entry* entry = resolveEntry(name);
        if (entry == nullptr || !entry->value) {
            return defaultValue;
        }

        const T* value = std::any_cast<T>(entry->value.get());
        if (value == nullptr) {
            logTypeMismatch(name, entry->value->type().name(), typeid(T).name(), m_writeScope);
            return defaultValue;
        }
        return *value;
    }

    /**
     * @brief Returns a scoped value or a fallback when the entry is unavailable.
     *
     * This overload is the explicit-scope counterpart of the optional getter.
     * It is used when a node wants to read another scope but can still operate
     * with a default if that value was not provided.
     *
     * @tparam T            Expected value type.
     * @param scope         Scope name.
     * @param name          Logical key name.
     * @param defaultValue  Fallback value.
     * @return Stored value or @p defaultValue.
     */
    template <typename T> T get(const std::string& scope, const std::string& name, const T& defaultValue) const
    {
        const Entry* entry = resolveScopedEntry(scope, name);
        if (entry == nullptr || !entry->value) {
            return defaultValue;
        }

        const T* value = std::any_cast<T>(entry->value.get());
        if (value == nullptr) {
            logTypeMismatch(qualifiedKey(scope, name), entry->value->type().name(), typeid(T).name(), m_writeScope);
            return defaultValue;
        }
        return *value;
    }

    /**
     * @brief Checks whether a value exists for the given key.
     *
     * This is a convenience helper for code paths that want to branch on
     * presence without fetching the value immediately.
     *
     * @param name Key to check.
     * @return @c true if an entry exists under @p name.
     */
    bool contains(const std::string& name) const;

    /**
     * @brief Returns the raw type-erased value for a key, if present.
     *
     * This helper is intended for generic serialization paths where the caller
     * needs to inspect the stored runtime type without triggering typed access
     * logs.
     *
     * @param name Qualified or unqualified key.
     * @return Pointer to stored std::any value, or @c nullptr if absent.
     */
    const std::any* valueAny(const std::string& name) const;

    /**
     * @brief Returns the raw type-erased value for an explicit scope/key pair.
     *
     * @param scope Scope name.
     * @param name  Key name inside scope.
     * @return Pointer to stored std::any value, or @c nullptr if absent.
     */
    const std::any* valueAny(const std::string& scope, const std::string& name) const;

    /**
     * @brief Returns all keys currently stored in this context.
     *
     * The returned keys are fully qualified and reflect all scopes currently
     * present in the context. The order is implementation-defined.
     *
     * @return Vector of key strings in unspecified order.
     */
    std::vector<std::string> keys() const;

    /**
     * @brief Merges all entries from another context into this one.
     *
     * The merge is shallow: shared value objects are copied by reference, not by
     * value. Existing keys are overwritten by the incoming context, which is the
     * desired behavior when two upstream branches converge and the later merge
     * should provide the latest available values.
     *
     * @param other The source context whose entries are merged into this one.
     */
    void mergeFrom(const FrameContext& other);

    /**
     * @brief Removes all stored entries and clears the scope order.
     *
     * After this call the context is empty and behaves like a fresh instance.
     */
    void clear();

    /**
     * @brief Removes one complete scope and all keys stored under it.
     *
     * This is primarily used by runtime retention logic to drop no-longer-needed
     * producer scopes after all dependent nodes have consumed them.
     *
     * @param scope Scope identifier to remove.
     * @return @c true when the scope existed and was removed.
     */
    bool eraseScope(const std::string& scope);

private:
    struct QualifiedName
    {
        std::string scope;
        std::string key;
    };

    /// Splits `scope.key` into scope/key parts. Unqualified names keep empty scope.
    static QualifiedName splitQualifiedName(const std::string& name);

    /// Builds a fully qualified key from scope and name.
    static std::string qualifiedKey(const std::string& scope, const std::string& name);

    /// Resolves key lookup with scope-order handling.
    const Entry* resolveEntry(const std::string& name) const;

    /// Mutable overload of resolveEntry.
    Entry* resolveEntry(const std::string& name);

    /// Resolves a key in an explicitly specified scope.
    const Entry* resolveScopedEntry(const std::string& scope, const std::string& name) const;

    /// Mutable overload of resolveScopedEntry.
    Entry* resolveScopedEntry(const std::string& scope, const std::string& name);

    /**
     * @brief Marks a scope as recently written in the current execution order.
     *
     * The scope stack is used to resolve unqualified reads backwards, so this
     * helper moves the supplied scope to the end of the order when new data is
     * written into it.
     *
     * @param scope Scope name to move to the most recent position.
     */
    void touchScope(const std::string& scope);

    /// Logs a lookup failure.
    static void logResolveFailure(const std::string& key, const std::string& requesterNode)
    {
        const std::string nodeText = requesterNode.empty() ? std::string("<unknown>") : requesterNode;
        LOG_ERROR("FrameContext key not found: '" + key + "' (node='" + nodeText + "')");
    }

    /// Logs a type mismatch for an existing context entry.
    static void logTypeMismatch(const std::string& key, const std::string& actualType, const std::string& expectedType, const std::string& requesterNode)
    {
        const std::string nodeText = requesterNode.empty() ? std::string("<unknown>") : requesterNode;
        LOG_ERROR("FrameContext type mismatch for '" + key + "' (expected " + expectedType + ", got " + actualType + ", node='" + nodeText + "')");
    }

    std::map<std::string, std::map<std::string, Entry>> m_scopes; ///< Stored values grouped by scope.
    std::string m_writeScope;                                     ///< Scope used for unqualified writes.
    std::vector<std::string> m_readScopes;                        ///< Scope order used for unqualified reads.
};
