#include "kadoka/relabel/batch_file_store.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace kadoka::relabel {
namespace {

constexpr std::uintmax_t max_record_bytes = std::uintmax_t{16} * 1024U * 1024U;
constexpr std::size_t max_evaluations = 1024;

// {
//   責務: [RecordReadExpectation: 読取対象のrevisionとcursorを束ねる],
//   フィールド: [revision: 期待するqueue revision] [next_index: 期待する次index]
// }
struct RecordReadExpectation {
    std::string revision;
    std::size_t next_index = 0;
};

// {
//   責務: [record_path: indexから固定桁数の確定ファイル名を作る],
//   処理: [1: indexを16桁の10進数へ変換する],
//   引数: [directory: 保存先] [index: sample位置],
//   戻り値: [path: 確定ファイルのパス]
// }
std::filesystem::path record_path(const std::filesystem::path& directory, const std::size_t index) {
    std::ostringstream name;
    name.imbue(std::locale::classic());
    name << std::setw(16) << std::setfill('0') << index << ".batch";
    return directory / name.str();
}

// {
//   責務: [read_value: 形式違反を即座に拒否しながら数値を読む],
//   処理: [1: 入力から数値を読む] [2: 読取失敗なら例外にする],
//   引数: [input: recordストリーム],
//   戻り値: [value: 読み取った数値]
// }
template <typename T> T read_value(std::istream& input) {
    T value{};
    if (!(input >> value))
        throw std::runtime_error("invalid relabel batch record");
    return value;
}

// {
//   責務: [read_string: quoted文字列を欠落なく読む],
//   処理: [1: quoted形式から復元する],
//   引数: [input: recordストリーム],
//   戻り値: [value: 復元した文字列]
// }
std::string read_string(std::istream& input) {
    std::string value;
    if (!(input >> std::quoted(value)))
        throw std::runtime_error("invalid relabel batch string");
    return value;
}

// {
//   責務: [read_flag: 任意値の有無を0/1で検証する],
//   処理: [1: 数値を読み0または1だけ受け入れる],
//   引数: [input: recordストリーム],
//   戻り値: [flag: 有無]
// }
bool read_flag(std::istream& input) {
    const auto value = read_value<unsigned>(input);
    if (value > 1)
        throw std::runtime_error("invalid relabel batch flag");
    return value == 1;
}

// {
//   責務: [write_optional_string: 任意文字列を形式付きで書く],
//   処理: [1: 有無を記録し、値がある場合はquoted形式で書く],
//   引数: [output: 出力先] [value: 任意文字列],
//   戻り値: []
// }
void write_optional_string(std::ostream& output, const std::optional<std::string>& value) {
    output << value.has_value() << ' ';
    if (value)
        output << std::quoted(*value) << ' ';
}

// {
//   責務: [read_optional_string: 任意文字列を形式付きで読む],
//   処理: [1: 有無を検証し、値がある場合はquoted形式で読む],
//   引数: [input: recordストリーム],
//   戻り値: [value: 復元した任意文字列]
// }
std::optional<std::string> read_optional_string(std::istream& input) {
    if (!read_flag(input))
        return std::nullopt;
    return read_string(input);
}

// {
//   責務: [serialize: revision・cursor・record全体を同じバイト列へ格納する],
//   処理: [1: versionと識別子を書く] [2: sourceを書く] [3: AIごとの結果を書く],
//   引数: [revision: queue識別子] [next_index: 保存後cursor] [record: 評価結果],
//   戻り値: [payload: 完全なrecord表現]
// }
std::string serialize(const std::string& revision, const std::size_t next_index, const RelabelRecord& record) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "KRB1 " << std::quoted(revision) << ' ' << next_index << '\n';
    const auto& source = record.source;
    output << std::quoted(source.dataset_id) << ' ' << std::quoted(source.dataset_revision) << ' '
           << std::quoted(source.sample_id) << ' ' << std::quoted(source.position_sfen) << ' ';
    write_optional_string(output, source.observed_move_usi);
    output << source.observed_score_cp.has_value() << ' ';
    if (source.observed_score_cp)
        output << *source.observed_score_cp << ' ';
    output << record.evaluations.size() << '\n';
    for (const auto& evaluation : record.evaluations) {
        output << std::quoted(evaluation.participant.value) << ' ' << std::quoted(evaluation.model_version) << ' '
               << std::quoted(evaluation.config_hash) << ' ' << static_cast<unsigned>(evaluation.status) << ' '
               << evaluation.turn_status.has_value() << ' ';
        if (evaluation.turn_status)
            output << static_cast<unsigned>(*evaluation.turn_status) << ' ';
        output << evaluation.search_result.has_value() << ' ';
        if (evaluation.search_result) {
            const auto& search = *evaluation.search_result;
            output << search.best_move.from.has_value() << ' ';
            if (search.best_move.from)
                output << static_cast<unsigned>(search.best_move.from->file) << ' '
                       << static_cast<unsigned>(search.best_move.from->rank) << ' ';
            output << static_cast<unsigned>(search.best_move.to.file) << ' '
                   << static_cast<unsigned>(search.best_move.to.rank) << ' '
                   << static_cast<unsigned>(search.best_move.drop_piece) << ' ' << search.best_move.promote << ' '
                   << search.score_cp << ' ' << search.nodes << ' ' << search.depth << ' ' << std::quoted(search.info)
                   << ' ' << static_cast<unsigned>(search.action) << ' ';
        }
        output << evaluation.decision_time.count() << ' ';
        write_optional_string(output, evaluation.error);
        output << '\n';
    }
    return output.str();
}

// {
//   責務: [deserialize: 保存ファイルを厳格に読み元recordとcursorを復元する],
//   処理: [1: version・revision・indexを照合する] [2: sourceとAI結果を読む]
//     [3: 余剰データを拒否する],
//   引数: [payload: 保存ファイル内容] [expected: 期待revisionとcursor],
//   戻り値: [record: 元sourceと全AI結果]
// }
RelabelRecord deserialize(const std::string& payload, const RecordReadExpectation& expected) {
    std::istringstream input(payload);
    input.imbue(std::locale::classic());
    std::string magic;
    if (!(input >> magic) || magic != "KRB1" || read_string(input) != expected.revision ||
        read_value<std::uint64_t>(input) != expected.next_index)
        throw std::runtime_error("relabel batch revision or index mismatch");
    RelabelRecord record;
    auto& source = record.source;
    source.dataset_id = read_string(input);
    source.dataset_revision = read_string(input);
    source.sample_id = read_string(input);
    source.position_sfen = read_string(input);
    source.observed_move_usi = read_optional_string(input);
    if (read_flag(input))
        source.observed_score_cp = read_value<std::int32_t>(input);
    const auto count = read_value<std::size_t>(input);
    if (count > max_evaluations)
        throw std::runtime_error("too many relabel evaluations");
    record.evaluations.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        RelabelEvaluation evaluation;
        evaluation.participant.value = read_string(input);
        evaluation.model_version = read_string(input);
        evaluation.config_hash = read_string(input);
        const auto status = read_value<unsigned>(input);
        if (status > static_cast<unsigned>(RelabelEvaluationStatus::failed))
            throw std::runtime_error("invalid relabel evaluation status");
        evaluation.status = static_cast<RelabelEvaluationStatus>(status);
        if (read_flag(input)) {
            const auto turn = read_value<unsigned>(input);
            if (turn > static_cast<unsigned>(shogi::runtime::TurnStatus::NoLegalMoves))
                throw std::runtime_error("invalid relabel turn status");
            evaluation.turn_status = static_cast<shogi::runtime::TurnStatus>(turn);
        }
        if (read_flag(input)) {
            shogi::SearchResult search;
            if (read_flag(input)) {
                shogi::Square from;
                const auto file = read_value<unsigned>(input);
                const auto rank = read_value<unsigned>(input);
                if (file > 9 || rank > 9)
                    throw std::runtime_error("invalid relabel move origin");
                from.file = static_cast<std::uint8_t>(file);
                from.rank = static_cast<std::uint8_t>(rank);
                search.best_move.from = from;
            }
            const auto file = read_value<unsigned>(input);
            const auto rank = read_value<unsigned>(input);
            if (file > 9 || rank > 9)
                throw std::runtime_error("invalid relabel move destination");
            search.best_move.to.file = static_cast<std::uint8_t>(file);
            search.best_move.to.rank = static_cast<std::uint8_t>(rank);
            const auto piece = read_value<unsigned>(input);
            if (piece > static_cast<unsigned>(shogi::PieceType::Dragon))
                throw std::runtime_error("invalid relabel move piece");
            search.best_move.drop_piece = static_cast<shogi::PieceType>(piece);
            search.best_move.promote = read_flag(input);
            search.score_cp = read_value<std::int32_t>(input);
            search.nodes = read_value<std::uint64_t>(input);
            search.depth = read_value<unsigned>(input);
            search.info = read_string(input);
            const auto action = read_value<unsigned>(input);
            if (action > static_cast<unsigned>(shogi::EngineAction::OfferMutualImpasse))
                throw std::runtime_error("invalid relabel action");
            search.action = static_cast<shogi::EngineAction>(action);
            evaluation.search_result = std::move(search);
        }
        evaluation.decision_time = std::chrono::nanoseconds{read_value<std::int64_t>(input)};
        evaluation.error = read_optional_string(input);
        record.evaluations.push_back(std::move(evaluation));
    }
    input >> std::ws;
    if (!input.eof())
        throw std::runtime_error("trailing relabel batch data");
    return record;
}

// {
//   責務: [read_record: サイズを制限して確定ファイルを復元する],
//   処理: [1: ファイルサイズを確認する] [2: 全バイトを読む] [3: 内容を検証する],
//   引数: [path: 確定ファイル] [revision: 期待revision] [next_index: 期待cursor],
//   戻り値: [record: 復元結果]
// }
RelabelRecord read_record(const std::filesystem::path& path, const std::string& revision,
                          const std::size_t next_index) {
    const auto size = std::filesystem::file_size(path);
    if (size > max_record_bytes)
        throw std::runtime_error("relabel batch record exceeds size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open relabel batch record");
    std::string payload(static_cast<std::size_t>(size), '\0');
    if (!input.read(payload.data(), static_cast<std::streamsize>(size)))
        throw std::runtime_error("cannot read complete relabel batch record");
    return deserialize(payload, {revision, next_index});
}

} // namespace

// {
//   責務: [RelabelBatchFileStore: 保存済み連続recordを検証しcursorを復元する],
//   処理: [1: ディレクトリ作成] [2: record番号の連続性と内容を検証する],
//   引数: [directory: queue専用保存先] [revision: queue識別子],
//   戻り値: [store: 復元済み保存先]
// }
RelabelBatchFileStore::RelabelBatchFileStore(std::filesystem::path directory, std::string revision)
    : directory_(std::move(directory)), revision_(std::move(revision)) {
    if (revision_.empty())
        throw std::invalid_argument("relabel batch revision is required");
    std::filesystem::create_directories(directory_);
    std::vector<std::size_t> indices;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        if (entry.path().extension() != ".batch")
            continue;
        const auto stem = entry.path().stem().string();
        if (stem.size() != 16 || stem.find_first_not_of("0123456789") != std::string::npos)
            throw std::runtime_error("invalid relabel batch filename");
        const auto index = std::stoull(stem);
        if (index >= std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("invalid relabel batch index");
        indices.push_back(static_cast<std::size_t>(index));
    }
    std::sort(indices.begin(), indices.end());
    for (std::size_t index = 0; index < indices.size(); ++index) {
        if (indices[index] != index)
            throw std::runtime_error("relabel batch record sequence has a gap");
        (void)read_record(record_path(directory_, index), revision_, index + 1);
    }
    next_index_ = indices.size();
}

// {
//   責務: [cursor: 連続して確定したrecord数を再開位置として返す],
//   処理: [1: revisionと次indexを組み立てる],
//   引数: [],
//   戻り値: [cursor: 保存済みcursor]
// }
RelabelBatchCursor RelabelBatchFileStore::cursor() const {
    return {revision_, next_index_};
}

// {
//   責務: [save: 1つのrecordと次cursorを同じファイルとして確定する],
//   処理: [1: 順序を確認] [2: 一時ファイルへ書く] [3: renameで確定],
//   引数: [record: 保存する結果] [next: 次cursor],
//   戻り値: [saved: 保存成功ならtrue]
// }
bool RelabelBatchFileStore::save(const RelabelRecord& record, const RelabelBatchCursor& next) {
    if (next.queue_revision != revision_ || next_index_ == std::numeric_limits<std::size_t>::max() ||
        next.next_index != next_index_ + 1)
        throw std::invalid_argument("relabel batch cursor is out of order");
    const auto final_path = record_path(directory_, next_index_);
    const auto temporary_path = final_path.string() + ".tmp";
    if (std::filesystem::exists(final_path))
        throw std::runtime_error("relabel batch record already exists");
    const auto payload = serialize(revision_, next.next_index, record);
    if (payload.size() > max_record_bytes)
        throw std::invalid_argument("relabel batch record exceeds size limit");
    try {
        {
            std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
            if (!output)
                return false;
            output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
            output.flush();
            if (!output)
                return false;
            output.close();
            if (!output)
                return false;
        }
        std::filesystem::rename(temporary_path, final_path);
    } catch (const std::filesystem::filesystem_error&) {
        return false;
    }
    ++next_index_;
    return true;
}

// {
//   責務: [load: 確定済みsampleの全情報を読み直す],
//   処理: [1: indexの範囲を検証する] [2: recordを復元する],
//   引数: [index: sample位置],
//   戻り値: [record: 復元済み結果]
// }
RelabelRecord RelabelBatchFileStore::load(const std::size_t index) const {
    if (index >= next_index_)
        throw std::out_of_range("relabel batch sample is not saved");
    return read_record(record_path(directory_, index), revision_, index + 1);
}

} // namespace kadoka::relabel
