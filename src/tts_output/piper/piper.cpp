#include "piper.h"
#include "piper_impl.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

#include <cpp-pinyin/Pinyin.h>
#include <cpp-pinyin/G2pglobal.h>

using json = nlohmann::json;

/* ------------------------------------------------------------------ */
/* 辅助：检测文本是否包含 CJK 汉字（U+4E00 ~ U+9FFF）                  */
/* ------------------------------------------------------------------ */
static bool contains_chinese(const char *text)
{
    std::string_view sv(text);
    auto text_view = una::ranges::utf8_view{sv};
    for (auto cp : text_view) {
        if (cp >= 0x4E00 && cp <= 0x9FFF) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* 辅助：单个数字字符转中文                                            */
/* ------------------------------------------------------------------ */
static std::string digit_to_chinese(char d)
{
    switch (d) {
        case '0': return "零";
        case '1': return "一";
        case '2': return "二";
        case '3': return "三";
        case '4': return "四";
        case '5': return "五";
        case '6': return "六";
        case '7': return "七";
        case '8': return "八";
        case '9': return "九";
        default:  return "";
    }
}

/* ------------------------------------------------------------------ */
/* 辅助：阿拉伯数字串转中文读法（支持小数点）                          */
/* ------------------------------------------------------------------ */
static std::string number_to_chinese(const std::string &num)
{
    size_t dot_pos = num.find('.');
    std::string int_part = (dot_pos == std::string::npos) ? num : num.substr(0, dot_pos);
    std::string frac_part = (dot_pos == std::string::npos) ? "" : num.substr(dot_pos + 1);

    // 整数部分：为简化且保持鲁棒，采用逐位读法（如年份、电话号码）
    std::string result;
    for (char c : int_part) {
        result += digit_to_chinese(c);
    }

    if (!frac_part.empty()) {
        result += "点";
        for (char c : frac_part) {
            result += digit_to_chinese(c);
        }
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* 辅助：文本规范化，将阿拉伯数字、常见数学符号、计量单位转为中文      */
/* ------------------------------------------------------------------ */
static std::string normalize_text(const std::string &text)
{
    std::string result;
    size_t i = 0;
    size_t n = text.size();

    // 单位/符号映射表（从长到短匹配，避免 m 挡住 km）
    static const std::vector<std::pair<std::string, std::string>> replacements = {
        // 复合单位（必须排在简单单位前面）
        {"km/h", "千米每小时"}, {"m/s", "米每秒"}, {"m²", "平方米"},
        {"m2", "平方米"}, {"㎡", "平方米"}, {"m³", "立方米"},
        {"m3", "立方米"},
        // 长度
        {"km", "千米"}, {"cm", "厘米"}, {"mm", "毫米"}, {"m", "米"},
        // 质量
        {"kg", "千克"}, {"mg", "毫克"}, {"g", "克"},
        // 容量
        {"ml", "毫升"}, {"L", "升"}, {"l", "升"},
        // 时间
        {"min", "分钟"}, {"h", "小时"}, {"s", "秒"},
        // 温度/角度
        {"℃", "摄氏度"}, {"°F", "华氏度"}, {"°", "度"},
        // 数学比较/运算
        {"<=", "小于等于"}, {">=", "大于等于"}, {"!=", "不等于"},
        {"==", "等于"}, {"÷", "除以"}, {"×", "乘以"}, {"*", "乘"},
        {"/", "除以"}, {"+", "加"}, {"-", "减"}, {"=", "等于"},
        {"<", "小于"}, {">", "大于"}, {"%", "百分之"},
        {"√", "根号"}, {"π", "派"}, {"∑", "求和"}, {"∞", "无穷大"},
        // 括号
        {"(", "左括号"}, {")", "右括号"}, {"[", "左方括号"},
        {"]", "右方括号"}, {"{", "左花括号"}, {"}", "右花括号"},
    };

    auto match_replacement = [&](size_t pos) -> const std::pair<std::string, std::string> * {
        const std::pair<std::string, std::string> *best = nullptr;
        for (const auto &p : replacements) {
            if (text.compare(pos, p.first.size(), p.first) == 0) {
                if (!best || p.first.size() > best->first.size()) {
                    best = &p;
                }
            }
        }
        return best;
    };

    while (i < n) {
        // 1. 优先匹配多字符单位/符号
        auto rep = match_replacement(i);
        if (rep) {
            result += rep->second;
            i += rep->first.size();
            continue;
        }

        // 2. 数字处理（支持小数点）
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= '0' && c <= '9') {
            size_t j = i;
            bool has_dot = false;
            while (j < n) {
                unsigned char d = static_cast<unsigned char>(text[j]);
                if (d >= '0' && d <= '9') {
                    j++;
                } else if (d == '.' && !has_dot) {
                    has_dot = true;
                    j++;
                } else {
                    break;
                }
            }
            result += number_to_chinese(text.substr(i, j - i));
            i = j;
            continue;
        }

        // 3. 普通字符原样保留
        result += text[i];
        i++;
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* 辅助：将单个拼音音节拆为声母+韵母+声调                            */
/* 与 Piper 中文训练预处理保持一致：initial final tone                */
/* ------------------------------------------------------------------ */
static std::vector<std::string> split_pinyin_syllable(const std::string &syllable)
{
    std::vector<std::string> result;
    if (syllable.empty()) {
        return result;
    }

    // 声调数字一定在最后
    std::string base = syllable;
    std::string tone;
    if (!base.empty() && base.back() >= '1' && base.back() <= '5') {
        tone = std::string(1, base.back());
        base.pop_back();
    }

    // 声母表（从长到短匹配，确保 "zh" 优先于 "z"）
    static const std::vector<std::string> initials = {
        "zh", "ch", "sh", "b", "p", "m", "f", "d", "t", "n", "l",
        "g", "k", "h", "j", "q", "x", "r", "z", "c", "s", "y", "w"
    };

    // 韵母表（从长到短匹配）
    static const std::vector<std::string> finals = {
        "iang", "iong", "uang", "ueng", "ian", "iao", "uai", "uan",
        "ang", "eng", "ing", "ong", "ia", "ie", "iu", "ua", "uo",
        "ui", "un", "ve", "vn", "an", "en", "in", "ai", "ei", "ao",
        "ou", "er", "a", "o", "e", "i", "u", "v", "ue"
    };

    std::string initial;
    std::string final_part = base;
    for (const auto &ini : initials) {
        if (base.compare(0, ini.size(), ini) == 0) {
            initial = ini;
            final_part = base.substr(ini.size());
            break;
        }
    }

    std::string matched_final;
    for (const auto &fin : finals) {
        if (final_part == fin) {
            matched_final = fin;
            break;
        }
    }

    // 零声母：无声母时用 "Ø" 占位
    if (initial.empty()) {
        result.push_back("Ø");
    } else {
        result.push_back(initial);
    }

    if (!matched_final.empty()) {
        result.push_back(matched_final);
    } else if (!final_part.empty()) {
        // _fallback：逐字符输出韵母，保证不丢失信息
        for (char c : final_part) {
            result.emplace_back(1, c);
        }
    }

    if (!tone.empty()) {
        result.push_back(tone);
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* 辅助：获取可执行文件所在目录，用于定位 res/dict 资源                */
/* ------------------------------------------------------------------ */
#include <unistd.h>

static std::string get_executable_dir()
{
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len < 0) {
        return "";
    }
    buf[len] = '\0';
    std::filesystem::path p(buf);
    return p.parent_path().string();
}

/* ------------------------------------------------------------------ */
/* piper_create                                                       */
/* ------------------------------------------------------------------ */
struct piper_synthesizer *piper_create(const char *model_path,
                                       const char *config_path,
                                       const char *espeak_data_path)
{
    if (!model_path) {
        return nullptr;
    }

    /* cpp-pinyin 字典路径：优先使用可执行文件所在目录下的 res/dict */
    {
        std::string exe_dir = get_executable_dir();
        if (!exe_dir.empty()) {
            std::filesystem::path candidate =
                std::filesystem::path(exe_dir) / "res" / "dict";
            if (std::filesystem::exists(candidate)) {
                Pinyin::setDictionaryPath(candidate);
            } else {
                Pinyin::setDictionaryPath(std::filesystem::path(exe_dir).parent_path() / "res" / "dict");
            }
        } else {
            Pinyin::setDictionaryPath("res/dict");
        }
    }

    std::string config_path_str;
    if (!config_path) {
        std::string model_path_str(model_path);
        config_path_str = model_path_str + ".json";
    } else {
        config_path_str = config_path;
    }

    std::ifstream config_stream(config_path_str);
    auto config = json::parse(config_stream);

    piper_synthesizer *synth = new piper_synthesizer();

    /* Load config options */
    if (config.contains("audio")) {
        auto &audio_obj = config["audio"];
        if (audio_obj.contains("sample_rate")) {
            synth->sample_rate = audio_obj["sample_rate"].get<int>();
        }
    }
    /* phoneme_id_map — 使用完整的音素字符串作为键 */
    if (config.contains("phoneme_id_map")) {
        auto &phoneme_id_map_value = config["phoneme_id_map"];
        for (auto &from_phoneme_item : phoneme_id_map_value.items()) {
            std::string from_phoneme = from_phoneme_item.key();
            for (auto &to_id_value : from_phoneme_item.value()) {
                PhonemeId to_id = to_id_value.get<PhonemeId>();
                synth->phoneme_id_map[from_phoneme].push_back(to_id);
            }
        }
    }

    synth->num_speakers = config["num_speakers"].get<SpeakerId>();

    if (config.contains("inference")) {
        auto inference_value = config["inference"];
        if (inference_value.contains("noise_scale")) {
            synth->synth_noise_scale = inference_value["noise_scale"].get<float>();
        }
        if (inference_value.contains("length_scale")) {
            synth->synth_length_scale = inference_value["length_scale"].get<float>();
        }
        if (inference_value.contains("noise_w")) {
            synth->synth_noise_w_scale = inference_value["noise_w"].get<float>();
        }
    }

    /* Load ONNX model */
    synth->session_options.DisableCpuMemArena();
    synth->session_options.DisableMemPattern();
    synth->session_options.DisableProfiling();

    synth->session = std::make_unique<Ort::Session>(
        Ort::Session(ort_env, model_path, synth->session_options));

    return synth;
}

/* ------------------------------------------------------------------ */
/* piper_free                                                         */
/* ------------------------------------------------------------------ */
void piper_free(struct piper_synthesizer *synth)
{
    if (!synth) {
        return;
    }
    delete synth;
}

/* ------------------------------------------------------------------ */
/* piper_default_synthesize_options                                   */
/* ------------------------------------------------------------------ */
piper_synthesize_options
piper_default_synthesize_options(piper_synthesizer *synth)
{
    piper_synthesize_options options;
    options.speaker_id = 0;
    options.length_scale = DEFAULT_LENGTH_SCALE;
    options.noise_scale = DEFAULT_NOISE_SCALE;
    options.noise_w_scale = DEFAULT_NOISE_W_SCALE;

    if (synth) {
        options.length_scale = synth->synth_length_scale;
        options.noise_scale = synth->synth_noise_scale;
        options.noise_w_scale = synth->synth_noise_w_scale;
    }
    return options;
}

/* ------------------------------------------------------------------ */
/* piper_synthesize_start                                             */
/* ------------------------------------------------------------------ */
int piper_synthesize_start(struct piper_synthesizer *synth, const char *text,
                           const piper_synthesize_options *options)
{
    if (!synth) {
        return PIPER_ERR_GENERIC;
    }

    /* Clear state */
    while (!synth->phoneme_id_queue.empty()) {
        synth->phoneme_id_queue.pop();
    }
    synth->chunk_samples.clear();

    /* Options */
    std::unique_ptr<piper_synthesize_options> default_options;
    if (!options) {
        default_options = std::make_unique<piper_synthesize_options>(
            piper_default_synthesize_options(synth));
        options = default_options.get();
    }

    synth->length_scale = options->length_scale;
    synth->noise_scale = options->noise_scale;
    synth->noise_w_scale = options->noise_w_scale;
    synth->speaker_id = options->speaker_id;

    /* 本程序仅支持中文朗读：先对数字/符号/单位做文本规范化 */
    std::string normalized = normalize_text(text);
    if (!contains_chinese(normalized.c_str())) {
        return PIPER_ERR_GENERIC;
    }

    std::vector<std::string> sentence_phonemes;

    /* 中文文本 → cpp-pinyin */
    {
        static std::unique_ptr<Pinyin::Pinyin> g2p;
        if (!g2p) {
            g2p = std::make_unique<Pinyin::Pinyin>();
        }

        auto res = g2p->hanziToPinyin(normalized.c_str(),
                                      Pinyin::ManTone::Style::TONE3,
                                      Pinyin::Error::Default,
                                      false, false, false);

        // 将每个拼音音节拆为声母+韵母+声调，再用空格连接
        std::vector<std::string> phoneme_tokens;
        for (const auto &s : res.toStdVector()) {
            // 跳过非拼音标记（标点符号原样保留，在 phoneme_id_map 中查找）
            if (s.empty()) {
                continue;
            }
            bool is_pinyin = false;
            for (char c : s) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    is_pinyin = true;
                    break;
                }
            }
            if (is_pinyin) {
                auto parts = split_pinyin_syllable(s);
                phoneme_tokens.insert(phoneme_tokens.end(), parts.begin(), parts.end());
            } else {
                // 标点、空格等保持原样
                phoneme_tokens.push_back(s);
            }
        }

        std::string phoneme_line;
        for (size_t i = 0; i < phoneme_tokens.size(); ++i) {
            if (i > 0) phoneme_line += " ";
            phoneme_line += phoneme_tokens[i];
        }
        sentence_phonemes.push_back(phoneme_line);
    }

    /* phoneme 字符串 → phoneme_id */
    for (auto &phonemes_str : sentence_phonemes) {
        if (phonemes_str.empty()) {
            continue;
        }

        std::vector<Phoneme> sentence_codepoints;
        std::vector<PhonemeId> sentence_ids;

        sentence_codepoints.push_back(PHONEME_BOS);
        sentence_ids.push_back(ID_BOS);

        // 对音素字符串做最长前缀匹配（支持多字符音素如 "zh", "ai", "ong"）
        auto phonemes_norm = una::norm::to_nfd_utf8(phonemes_str);
        size_t pos = 0;
        while (pos < phonemes_norm.size()) {
            // 跳过空格分隔符：空格不是音素，仅用于 human-readable
            if (phonemes_norm[pos] == ' ') {
                pos++;
                continue;
            }

            // 最长前缀匹配
            std::string best_match;
            size_t best_len = 0;
            for (const auto &[key, ids] : synth->phoneme_id_map) {
                if (!key.empty() && phonemes_norm.compare(pos, key.size(), key) == 0) {
                    if (key.size() > best_len) {
                        best_match = key;
                        best_len = key.size();
                    }
                }
            }

            if (best_len > 0) {
                auto ids_for_phoneme = synth->phoneme_id_map.find(best_match);
                if (ids_for_phoneme != synth->phoneme_id_map.end()) {
                    // 取匹配音素的首码点作为 phoneme 输出（兼容 char32_t API）
                    auto view = una::ranges::utf8_view{best_match};
                    Phoneme first_cp = *view.begin();
                    for (auto id : ids_for_phoneme->second) {
                        sentence_codepoints.push_back(first_cp);
                        sentence_ids.push_back(id);
                    }

                    // 中文 PINYIN 模式：在每个"音素组"（声调或标点）后插入 PAD
                    // 参考 piper/phonemize_chinese.py 的 phonemes_to_ids
                    char last_char = best_match.empty() ? '\0' : best_match.back();
                    bool is_group_end = false;
                    if (last_char >= '1' && last_char <= '5') {
                        is_group_end = true;
                    } else {
                        static const std::string group_end_chars =
                            "。.？?!！—…、，,:：;； ";
                        if (group_end_chars.find(best_match) != std::string::npos) {
                            is_group_end = true;
                        }
                    }
                    if (is_group_end) {
                        sentence_codepoints.push_back(PHONEME_PAD);
                        sentence_ids.push_back(ID_PAD);
                    }
                    sentence_codepoints.push_back(PHONEME_SEPARATOR);
                }
                pos += best_len;
            } else {
                // 未匹配到任何音素，按 UTF-8 首字节编码跳过当前字符
                unsigned char c = (unsigned char)phonemes_norm[pos];
                int skip = 1;
                if ((c & 0xF8) == 0xF0) skip = 4;
                else if ((c & 0xF0) == 0xE0) skip = 3;
                else if ((c & 0xE0) == 0xC0) skip = 2;
                pos += skip;
            }
        }

        sentence_codepoints.push_back(PHONEME_EOS);
        sentence_ids.push_back(ID_EOS);

        synth->phoneme_id_queue.emplace(
            std::make_pair(std::move(sentence_codepoints), std::move(sentence_ids)));
        sentence_codepoints.clear();
        sentence_ids.clear();
    }

    return PIPER_OK;
}

/* ------------------------------------------------------------------ */
/* piper_synthesize_next（完全不变）                                  */
/* ------------------------------------------------------------------ */
int piper_synthesize_next(struct piper_synthesizer *synth,
                          struct piper_audio_chunk *chunk)
{
    if (!synth) {
        return PIPER_ERR_GENERIC;
    }

    if (!chunk) {
        return PIPER_ERR_GENERIC;
    }

    /* Clear data from previous call */
    synth->chunk_samples.clear();
    synth->chunk_phonemes.clear();
    synth->chunk_phoneme_ids.clear();
    synth->chunk_alignments.clear();

    chunk->sample_rate = synth->sample_rate;
    chunk->samples = nullptr;
    chunk->num_samples = 0;
    chunk->is_last = false;
    chunk->phoneme_ids = nullptr;
    chunk->num_phoneme_ids = 0;
    chunk->alignments = nullptr;
    chunk->num_alignments = 0;

    if (synth->phoneme_id_queue.empty()) {
        chunk->is_last = true;
        return PIPER_DONE;
    }

    /* Process next list of phoneme ids */
    auto [next_phonemes, next_ids] = std::move(synth->phoneme_id_queue.front());
    synth->phoneme_id_queue.pop();

    auto memoryInfo = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

    /* Allocate */
    std::vector<int64_t> phoneme_id_lengths{(int64_t)next_ids.size()};
    std::vector<float> scales{synth->noise_scale, synth->length_scale,
                              synth->noise_w_scale};

    std::vector<Ort::Value> input_tensors;
    std::vector<int64_t> phoneme_ids_shape{1, (int64_t)next_ids.size()};
    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memoryInfo, next_ids.data(), next_ids.size(), phoneme_ids_shape.data(),
        phoneme_ids_shape.size()));

    std::vector<int64_t> phoneme_id_lengths_shape{
        (int64_t)phoneme_id_lengths.size()};
    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memoryInfo, phoneme_id_lengths.data(), phoneme_id_lengths.size(),
        phoneme_id_lengths_shape.data(), phoneme_id_lengths_shape.size()));

    std::vector<int64_t> scales_shape{(int64_t)scales.size()};
    input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo, scales.data(), scales.size(), scales_shape.data(),
        scales_shape.size()));

    /* Add speaker id */
    std::vector<int64_t> speaker_id{(int64_t)synth->speaker_id};
    std::vector<int64_t> speaker_id_shape{(int64_t)speaker_id.size()};

    if (synth->num_speakers > 1) {
        input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo, speaker_id.data(), speaker_id.size(),
            speaker_id_shape.data(), speaker_id_shape.size()));
    }

    std::array<const char *, 4> input_names = {"input", "input_lengths",
                                               "scales", "sid"};

    std::vector<std::string> output_names_strs =
        synth->session->GetOutputNames();
    std::vector<const char *> output_names;
    for (const auto &name : output_names_strs) {
        output_names.push_back(name.c_str());
    }

    /* Infer */
    auto output_tensors = synth->session->Run(
        Ort::RunOptions{nullptr}, input_names.data(), input_tensors.data(),
        input_tensors.size(), output_names.data(), output_names.size());

    if ((output_tensors.size() < 1) || (!output_tensors.front().IsTensor())) {
        return PIPER_ERR_GENERIC;
    }

    auto audio_shape =
        output_tensors.front().GetTensorTypeAndShapeInfo().GetShape();
    chunk->num_samples = audio_shape[audio_shape.size() - 1];

    const float *audio_tensor_data =
        output_tensors.front().GetTensorData<float>();
    synth->chunk_samples.resize(chunk->num_samples);
    std::copy(audio_tensor_data, audio_tensor_data + chunk->num_samples,
              synth->chunk_samples.begin());
    chunk->samples = synth->chunk_samples.data();

    chunk->is_last = synth->phoneme_id_queue.empty();

    /* Copy phonemes */
    synth->chunk_phonemes = std::move(next_phonemes);
    chunk->phonemes = synth->chunk_phonemes.data();
    chunk->num_phonemes = synth->chunk_phonemes.size();

    /* Copy phoneme ids */
    for (auto phoneme_id : next_ids) {
        if (phoneme_id < std::numeric_limits<int>::min() ||
            phoneme_id > std::numeric_limits<int>::max()) {
            continue;
        }
        synth->chunk_phoneme_ids.push_back(static_cast<int>(phoneme_id));
    }

    chunk->phoneme_ids = synth->chunk_phoneme_ids.data();
    chunk->num_phoneme_ids = synth->chunk_phoneme_ids.size();

    /* Check for alignments */
    if (output_tensors.size() > 1) {
        auto alignments_shape =
            output_tensors[1].GetTensorTypeAndShapeInfo().GetShape();

        chunk->num_alignments = alignments_shape[alignments_shape.size() - 1];
        const float *alignments_tensor_data =
            output_tensors[1].GetTensorData<float>();

        synth->chunk_alignments.resize(chunk->num_alignments);
        for (std::size_t i = 0; i < chunk->num_alignments; i++) {
            synth->chunk_alignments[i] =
                (int)(alignments_tensor_data[i] * synth->hop_length);
        }

        chunk->alignments = synth->chunk_alignments.data();
    }

    /* Clean up */
    for (std::size_t i = 0; i < output_tensors.size(); i++) {
        Ort::detail::OrtRelease(output_tensors[i].release());
    }

    for (std::size_t i = 0; i < input_tensors.size(); i++) {
        Ort::detail::OrtRelease(input_tensors[i].release());
    }

    return PIPER_OK;
}