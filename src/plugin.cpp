#include <D2RLPlugin/api.h>

#include "default_config.hpp"

#include <windows.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

#include <toml++/toml.hpp>

namespace {

constexpr char PluginId[] = "unique-rune-cube";
constexpr char ConvertTarget[] = "HoradricCubePanelMessage";
constexpr char ConvertCommand[] = "Convert";
constexpr std::size_t MaximumCubeItems = 12;
constexpr std::uint32_t RuneCount = 33;
constexpr std::uint32_t MaximumRuneOutputs = 3;
constexpr std::uint32_t RuneRarityStepPercent = 80;
constexpr wchar_t ConfigFileName[] = L"unique-rune-cube.toml";
constexpr std::array<std::string_view, RuneCount> RuneNames{
    "el", "eld", "tir", "nef", "eth", "ith", "tal", "ral", "ort",
    "thul", "amn", "sol", "shael", "dol", "hel", "io", "lum", "ko",
    "fal", "lem", "pul", "um", "mal", "ist", "gul", "vex", "ohm",
    "lo", "sur", "ber", "jah", "cham", "zod",
};

auto MakeDefaultRuneWeights() -> std::array<double, RuneCount> {
    std::array<double, RuneCount> weights{};
    double weight = 100.0;
    for (double& runeWeight : weights) {
        runeWeight = weight;
        weight *= static_cast<double>(RuneRarityStepPercent) / 100.0;
    }
    return weights;
}

struct Settings {
    std::uint32_t failureChancePercent{30};
    std::array<std::uint32_t, MaximumRuneOutputs> outputCountWeights{70, 25, 5};
    std::array<double, RuneCount> runeWeights = MakeDefaultRuneWeights();
};

struct PendingExchange {
    D2RL::PlayerHandle player{D2RL::InvalidPlayerHandle};
    std::uint32_t sourceCode{};
    std::int32_t sourceQualityRecordId{-1};
    D2RL::Items::Quality sourceQuality{D2RL::Items::Quality::Unknown};
};

struct CubeCollector {
    std::array<D2RL::Items::ItemInfo, MaximumCubeItems> items{};
    std::size_t count{};
    bool overflow{};
};

Settings Config;

constexpr D2RL::PluginInfo Info{
    .infoSize = D2RL::PluginInfoSize,
    .apiVersion = D2RL_PLUGIN_API_VERSION,
    .id = PluginId,
    .name = "Unique/Set Scrapper",
    .version = "0.3.1",
    .author = "MadMike",
    .description = "Transmutes one unique or set item into a chance at random runes.",
    .flags = D2RL::PluginFlags::Client,
};

const D2RL::PluginContext* Context{};
const D2RL::InventoryServiceV1* InventoryService{};
const D2RL::ItemServiceV1* ItemService{};
const D2RL::ThreadServiceV1* ThreadService{};
const D2RL::SharedEventServiceV1* EventService{};
D2RL::SharedEvents::ListenerHandle MessageHandle{D2RL::SharedEvents::InvalidHandle};
std::atomic<bool> Active{};
std::atomic<bool> ExchangePending{};
std::atomic<bool> ConvertMessageLogged{};
std::atomic<bool> MismatchMessageLogged{};
std::mutex PendingMutex;
PendingExchange Pending;
std::mt19937 RandomEngine;

auto __cdecl CollectCubeItem(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item,
        void* userData) noexcept -> D2RL::Inventory::IterationAction {
    auto* collector = static_cast<CubeCollector*>(userData);
    if (collector == nullptr || item == nullptr
            || item->structSize < D2RL::Items::ItemInfoRequiredSize) {
        return D2RL::Inventory::IterationAction::Continue;
    }
    if (collector->count >= collector->items.size()) {
        collector->overflow = true;
        return D2RL::Inventory::IterationAction::Stop;
    }
    collector->items[collector->count++] = *item;
    return D2RL::Inventory::IterationAction::Continue;
}

auto ReadCubeItems(D2RL::PlayerHandle player, CubeCollector& cube) noexcept
        -> D2RL::Inventory::Result {
    const D2RL::Inventory::ItemFilter filter{
        .structSize = D2RL::Inventory::ItemFilterSize,
        .flags = 0,
        .containerMask = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Cube),
        .reserved = 0,
    };
    return InventoryService->forEachInventoryItem(Context, player, &filter,
        CollectCubeItem, &cube);
}

constexpr auto IsAcceptedQuality(D2RL::Items::Quality quality) noexcept -> bool {
    return quality == D2RL::Items::Quality::Unique
        || quality == D2RL::Items::Quality::Set;
}

auto MatchesPending(const D2RL::Items::ItemInfo& item,
        const PendingExchange& pending) noexcept -> bool {
    return IsAcceptedQuality(item.quality)
        && item.quality == pending.sourceQuality
        && item.code == pending.sourceCode
        && item.qualityRecordId == pending.sourceQualityRecordId;
}

constexpr auto MakeRuneCode(std::uint32_t runeNumber) noexcept -> std::uint32_t {
    return D2RL::Items::MakeItemCode('r',
        static_cast<char>('0' + runeNumber / 10U),
        static_cast<char>('0' + runeNumber % 10U));
}

auto ConfigPath() -> std::filesystem::path {
    if (Context != nullptr && Context->pluginConfigPath != nullptr
            && Context->pluginConfigPath[0] != L'\0') {
        return std::filesystem::path(Context->pluginConfigPath);
    }
    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    if (error) return {};
    return current / L"d2rloader" / L"config" / ConfigFileName;
}

auto ReadInteger(const toml::table& table, std::string_view key,
        std::uint32_t minimum, std::uint32_t maximum,
        std::uint32_t& destination, std::string& error) -> bool {
    const auto* node = table.get(key);
    if (node == nullptr) return true;
    const auto value = node->value<std::int64_t>();
    if (!value || *value < minimum || *value > maximum) {
        error = std::string(key) + " must be an integer from "
            + std::to_string(minimum) + " through " + std::to_string(maximum);
        return false;
    }
    destination = static_cast<std::uint32_t>(*value);
    return true;
}

auto ReadRuneWeight(const toml::table& table, std::string_view key,
        double& destination, std::string& error) -> bool {
    const auto* node = table.get(key);
    if (node == nullptr) return true;
    double value{};
    if (const auto floating = node->value<double>()) {
        value = *floating;
    } else if (const auto integer = node->value<std::int64_t>()) {
        value = static_cast<double>(*integer);
    } else {
        error = std::string(key) + " must be a non-negative number";
        return false;
    }
    if (!std::isfinite(value) || value < 0.0 || value > 1.0e12) {
        error = std::string(key) + " must be finite and between 0 and 1e12";
        return false;
    }
    destination = value;
    return true;
}

auto ParseConfig(std::string_view text, Settings& destination,
        std::string& error) -> bool {
    try {
        const auto root = toml::parse(text);
        const auto* sectionNode = root.get("unique-rune-cube");
        if (sectionNode == nullptr) return true;
        const auto* section = sectionNode->as_table();
        if (section == nullptr) {
            error = "unique-rune-cube must be a TOML table";
            return false;
        }
        if (!ReadInteger(*section, "failure-chance-percent", 0, 100,
                destination.failureChancePercent, error)) {
            return false;
        }

        const auto* countNode = section->get("output-count-weights");
        if (countNode != nullptr) {
            const auto* counts = countNode->as_table();
            if (counts == nullptr) {
                error = "unique-rune-cube.output-count-weights must be a table";
                return false;
            }
            if (!ReadInteger(*counts, "one", 0, 100,
                    destination.outputCountWeights[0], error)
                    || !ReadInteger(*counts, "two", 0, 100,
                    destination.outputCountWeights[1], error)
                    || !ReadInteger(*counts, "three", 0, 100,
                    destination.outputCountWeights[2], error)) {
                return false;
            }
        }
        const std::uint32_t countWeightTotal = destination.outputCountWeights[0]
            + destination.outputCountWeights[1] + destination.outputCountWeights[2];
        if (countWeightTotal != 100) {
            error = "output-count-weights one, two, and three must sum to 100";
            return false;
        }

        const auto* runeNode = section->get("rune-weights");
        if (runeNode != nullptr) {
            const auto* runeWeights = runeNode->as_table();
            if (runeWeights == nullptr) {
                error = "unique-rune-cube.rune-weights must be a table";
                return false;
            }
            for (std::size_t index = 0; index < RuneNames.size(); ++index) {
                if (!ReadRuneWeight(*runeWeights, RuneNames[index],
                        destination.runeWeights[index], error)) {
                    return false;
                }
            }
        }
        double totalRuneWeight{};
        for (const double weight : destination.runeWeights) {
            totalRuneWeight += weight;
        }
        if (!std::isfinite(totalRuneWeight) || totalRuneWeight <= 0.0) {
            error = "at least one rune weight must be greater than zero";
            return false;
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

auto LoadConfig() noexcept -> bool {
    std::string configStorage;
    std::string_view text = DefaultConfig;
    try {
        const auto path = ConfigPath();
        if (!path.empty()) {
            std::error_code error;
            if (!std::filesystem::exists(path, error) && !error) {
                std::filesystem::create_directories(path.parent_path(), error);
                if (!error) {
                    std::ofstream output(path, std::ios::binary);
                    if (output.is_open()) {
                        output.write(DefaultConfig,
                            static_cast<std::streamsize>(sizeof(DefaultConfig) - 1));
                    }
                }
            }
            error.clear();
            if (std::filesystem::is_regular_file(path, error) && !error) {
                std::ifstream input(path, std::ios::binary);
                if (input.is_open()) {
                    configStorage.assign(std::istreambuf_iterator<char>(input),
                        std::istreambuf_iterator<char>());
                    if (!input.bad()) text = configStorage;
                } else {
                    Context->LogWarn("UniqueSetScrapper: config could not be opened; embedded defaults will be used.");
                }
            }
        }
    } catch (...) {
        Context->LogWarn("UniqueSetScrapper: config access failed; embedded defaults will be used.");
    }

    Settings parsed;
    std::string error;
    if (!ParseConfig(text, parsed, error)) {
        const std::string message = "UniqueSetScrapper: invalid TOML config (" + error + ").";
        Context->LogError(message.c_str());
        return false;
    }
    Config = parsed;
    return true;
}

void ResetPending() noexcept {
    Pending = {};
    ExchangePending.store(false, std::memory_order_release);
}

void __cdecl ExecuteExchange(const D2RL::PluginContext* context, void*) noexcept {
    const std::lock_guard<std::mutex> lock(PendingMutex);
    if (context == nullptr || context != Context || !Active.load(std::memory_order_acquire)) {
        ResetPending();
        return;
    }

    CubeCollector cube;
    const auto inventoryResult = ReadCubeItems(Pending.player, cube);
    if (inventoryResult != D2RL::Inventory::Result::Success) {
        char message[144]{};
        (void)std::snprintf(message, sizeof(message),
            "UniqueSetScrapper: could not re-read cube contents on the game thread (Inventory result=%u).",
            static_cast<unsigned>(inventoryResult));
        Context->LogWarn(message);
        ResetPending();
        return;
    }
    if (cube.overflow || cube.count != 1
            || !IsAcceptedQuality(cube.items[0].quality)) {
        char message[160]{};
        (void)std::snprintf(message, sizeof(message),
            "UniqueSetScrapper: game-thread recheck found %u cube item(s); exactly one unique or set item is required.",
            static_cast<unsigned>(cube.count));
        Context->LogWarn(message);
        ResetPending();
        return;
    }
    const D2RL::Items::ItemInfo& source = cube.items[0];
    if (!MatchesPending(source, Pending) || source.handle == D2RL::InvalidItemHandle) {
        Context->LogWarn("UniqueSetScrapper: the source item changed before the transaction; no items were changed.");
        ResetPending();
        return;
    }

    const std::uint32_t failureRoll =
        std::uniform_int_distribution<std::uint32_t>(1, 100)(RandomEngine);
    if (failureRoll <= Config.failureChancePercent) {
        const auto status = ItemService->destroyItem(context, Pending.player, source.handle,
            D2RL::Items::SocketedItemPolicy::DestroyContents);
        if (status == D2RL::Items::Result::Success) {
            char message[160]{};
            (void)std::snprintf(message, sizeof(message),
                "UniqueSetScrapper: recipe failed (%u%% roll); the item and socket contents were consumed, with no rune output.",
                Config.failureChancePercent);
            Context->LogInfo(message);
        } else {
            char message[176]{};
            (void)std::snprintf(message, sizeof(message),
                "UniqueSetScrapper: recipe failed, but ItemService could not consume the item (result=%u); the item was left untouched.",
                static_cast<unsigned>(status));
            Context->LogWarn(message);
        }
        ResetPending();
        return;
    }

    const std::uint32_t outputCount =
        std::discrete_distribution<std::uint32_t>(Config.outputCountWeights.begin(),
            Config.outputCountWeights.end())(RandomEngine) + 1;
    std::array<D2RL::Items::ItemCreateSpec, MaximumRuneOutputs> outputs{};
    std::array<D2RL::ItemHandle, MaximumRuneOutputs> outputItems{};
    for (std::uint32_t index = 0; index < outputCount; ++index) {
        auto& output = outputs[index];
        output.structSize = D2RL::Items::ItemCreateSpecSize;
        const std::uint32_t runeIndex =
            std::discrete_distribution<std::uint32_t>(Config.runeWeights.begin(),
                Config.runeWeights.end())(RandomEngine);
        output.code = MakeRuneCode(runeIndex + 1);
        output.quality = D2RL::Items::Quality::Normal;
        output.qualityRecordId = D2RL::Items::RandomQualityRecord;
        output.itemLevel = 1;
        output.seedMode = D2RL::Items::SeedMode::Random;
        output.quantity = D2RL::Items::DefaultValue;
        output.durability = D2RL::Items::DefaultValue;
        output.socketCount = 0;
        output.stateFlags = 0;
        output.destination = {
            .structSize = D2RL::Items::ItemDestinationSize,
            .flags = 0,
            .container = D2RL::Items::ItemContainer::Cube,
            .placement = D2RL::Items::Placement::Automatic,
            .customPageHandle = 0,
            .x = 0,
            .y = 0,
        };
    }

    const D2RL::Items::TransactionInput input{
        .item = source.handle,
        .quantity = 1,
        .socketedItemPolicy = D2RL::Items::SocketedItemPolicy::DestroyContents,
    };
    D2RL::Items::Transaction transaction{
        .structSize = D2RL::Items::TransactionSize,
        .flags = 0,
        .player = Pending.player,
        .inputCount = 1,
        .outputCount = outputCount,
        .inputs = &input,
        .outputs = outputs.data(),
        .outputItems = outputItems.data(),
        .outputCapacity = MaximumRuneOutputs,
        .reserved = 0,
    };
    D2RL::Items::TransactionResult result{
        .structSize = D2RL::Items::TransactionResultSize,
        .flags = 0,
        .outputCount = 0,
        .reserved = 0,
    };

    const auto status = ItemService->executeTransaction(context, &transaction, &result);
    if (status == D2RL::Items::Result::Success) {
        char message[128]{};
        (void)std::snprintf(message, sizeof(message),
            "UniqueSetScrapper: transmuted one unique or set item into %u random rune(s).", outputCount);
        Context->LogInfo(message);
        if (result.outputCount != outputCount) {
            Context->LogWarn("UniqueSetScrapper: ItemService committed the transaction but reported an unexpected output count.");
        }
    } else {
        char message[176]{};
        (void)std::snprintf(message, sizeof(message),
            "UniqueSetScrapper: transaction rolled back (ItemService result=%u); the item was left untouched.",
            static_cast<unsigned>(status));
        Context->LogWarn(message);
    }
    ResetPending();
}

auto __cdecl OnUiMessage(const D2RL::PluginContext* context,
        const D2RL::SharedEvents::UiMessageEvent* event, void*) noexcept
        -> D2RL::SharedEvents::UiMessageAction {
    if (context != Context || event == nullptr
            || event->structSize < D2RL::SharedEvents::UiMessageEventRequiredSize
            || event->target == nullptr || event->command == nullptr
            || std::string_view(event->target) != ConvertTarget
            || std::string_view(event->command) != ConvertCommand
            || !Active.load(std::memory_order_acquire)) {
        return D2RL::SharedEvents::UiMessageAction::Continue;
    }

    if (!ConvertMessageLogged.exchange(true, std::memory_order_acq_rel)) {
        Context->LogInfo("UniqueSetScrapper: observed the Horadric Cube Convert message.");
    }

    D2RL::PlayerHandle player{D2RL::InvalidPlayerHandle};
    if (InventoryService->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success
            || player == D2RL::InvalidPlayerHandle) {
        return D2RL::SharedEvents::UiMessageAction::Continue;
    }

    CubeCollector cube;
    if (ReadCubeItems(player, cube) != D2RL::Inventory::Result::Success
            || cube.overflow || cube.count != 1
            || !IsAcceptedQuality(cube.items[0].quality)
            || cube.items[0].handle == D2RL::InvalidItemHandle) {
        if (!MismatchMessageLogged.exchange(true, std::memory_order_acq_rel)) {
            Context->LogInfo("UniqueSetScrapper: Convert ignored; the cube must contain exactly one unique or set item and nothing else.");
        }
        return D2RL::SharedEvents::UiMessageAction::Continue;
    }
    const D2RL::Items::ItemInfo& source = cube.items[0];
    if (ExchangePending.exchange(true, std::memory_order_acq_rel)) {
        return D2RL::SharedEvents::UiMessageAction::Consume;
    }
    {
        const std::lock_guard<std::mutex> lock(PendingMutex);
        Pending = {
            .player = player,
            .sourceCode = source.code,
            .sourceQualityRecordId = source.qualityRecordId,
            .sourceQuality = source.quality,
        };
    }
    if (ThreadService->runOnGameThread(context, ExecuteExchange, nullptr)
            != D2RL::Threads::Result::Success) {
        const std::lock_guard<std::mutex> lock(PendingMutex);
        ResetPending();
        Context->LogWarn("UniqueSetScrapper: could not schedule the transaction on the game thread.");
        return D2RL::SharedEvents::UiMessageAction::Continue;
    }
    return D2RL::SharedEvents::UiMessageAction::Consume;
}

auto Initialize(const D2RL::PluginContext* context) -> bool {
    if (context->QueryService(D2RL::ServiceId::Inventory,
            D2RL::InventoryServiceV1Version, &InventoryService)
            != D2RL::ServiceQueryResult::Success
            || InventoryService == nullptr
            || InventoryService->serviceSize < D2RL::InventoryServiceV1RequiredSize
            || InventoryService->getLocalPlayer == nullptr
            || InventoryService->forEachInventoryItem == nullptr) {
        context->LogError("UniqueSetScrapper: D2RLoader InventoryService v1 is required.");
        return false;
    }
    if (context->QueryService(D2RL::ServiceId::Item,
            D2RL::ItemServiceV1Version, &ItemService)
            != D2RL::ServiceQueryResult::Success
            || ItemService == nullptr
            || ItemService->serviceSize < D2RL::ItemServiceV1RequiredSize
            || ItemService->destroyItem == nullptr
            || ItemService->executeTransaction == nullptr) {
        context->LogError("UniqueSetScrapper: D2RLoader ItemService v1 transaction support is required.");
        return false;
    }
    if (context->QueryService(D2RL::ServiceId::Thread,
            D2RL::ThreadServiceV1Version, &ThreadService)
            != D2RL::ServiceQueryResult::Success
            || ThreadService == nullptr
            || ThreadService->serviceSize < D2RL::ThreadServiceV1RequiredSize
            || ThreadService->runOnGameThread == nullptr) {
        context->LogError("UniqueSetScrapper: D2RLoader game-thread service is required.");
        return false;
    }
    if (context->QueryService(D2RL::ServiceId::SharedEvent,
            D2RL::SharedEventServiceV1Version, &EventService)
            != D2RL::ServiceQueryResult::Success
            || EventService == nullptr
            || EventService->serviceSize < D2RL::SharedEventServiceV1RequiredSize
            || EventService->registerUiMessageListener == nullptr
            || EventService->unregisterUiMessageListener == nullptr) {
        context->LogError("UniqueSetScrapper: D2RLoader shared UI-message service is required.");
        return false;
    }
    if (!LoadConfig()) return false;

    std::uint64_t seed = GetTickCount64()
        ^ (static_cast<std::uint64_t>(GetCurrentProcessId()) << 32U);
    try {
        std::random_device random;
        seed ^= (static_cast<std::uint64_t>(random()) << 32U) | random();
    } catch (...) {
    }
    RandomEngine.seed(static_cast<std::mt19937::result_type>(seed ^ (seed >> 32U)));

    const D2RL::SharedEvents::UiMessageListener listener{
        .structSize = D2RL::SharedEvents::UiMessageListenerSize,
        .flags = 0,
        .priority = 10000,
        .reserved = 0,
        .callback = OnUiMessage,
        .userData = nullptr,
    };
    if (EventService->registerUiMessageListener(context, &listener, &MessageHandle)
            != D2RL::SharedEvents::Result::Success
            || MessageHandle == D2RL::SharedEvents::InvalidHandle) {
        context->LogError("UniqueSetScrapper: could not register for the Horadric Cube Convert action.");
        return false;
    }
    Active.store(true, std::memory_order_release);
    context->LogInfo("UniqueSetScrapper 0.3.1 is ready; unique and set items are accepted, and all probabilities are loaded from unique-rune-cube.toml.");
    return true;
}

void Shutdown() noexcept {
    Active.store(false, std::memory_order_release);
    if (Context != nullptr && EventService != nullptr
            && MessageHandle != D2RL::SharedEvents::InvalidHandle
            && EventService->unregisterUiMessageListener != nullptr) {
        (void)EventService->unregisterUiMessageListener(Context, MessageHandle);
    }
    MessageHandle = D2RL::SharedEvents::InvalidHandle;
    const std::lock_guard<std::mutex> lock(PendingMutex);
    ResetPending();
    RandomEngine.seed(0);
    ConvertMessageLogged.store(false, std::memory_order_release);
    MismatchMessageLogged.store(false, std::memory_order_release);
    EventService = nullptr;
    ThreadService = nullptr;
    ItemService = nullptr;
    InventoryService = nullptr;
}

} // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
    return &Info;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
    Shutdown();
    if (!D2RL::HasContext(context) || context->apiVersion != D2RL_PLUGIN_API_VERSION) return false;
    Context = context;
    bool initialized{};
    try {
        initialized = Initialize(context);
    } catch (...) {
        Context->LogError("UniqueSetScrapper: plugin initialization failed.");
    }
    if (!initialized) {
        Shutdown();
        Context = nullptr;
        return false;
    }
    return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
    Shutdown();
    Context = nullptr;
}
