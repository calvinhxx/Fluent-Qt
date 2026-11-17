#ifndef FLUENTQT_COMPONENTS_DATE_TIME_DATETIMERANGEPICKERTEXT_P_H
#define FLUENTQT_COMPONENTS_DATE_TIME_DATETIMERANGEPICKERTEXT_P_H

#include "components/date_time/DateTimeRangePicker.h"

namespace fluent::date_time::detail {

// Prefer Qt translations; when none matches (Qt returns the source string), fall back
// to built-in Chinese text for Chinese locales unless an English locale was selected.
inline QString dateTimeRangeText(const QLocale& locale, const char* source)
{
    const QString translated = DateTimeRangePicker::tr(source);
    if (translated != QLatin1String(source) || locale.language() != QLocale::Chinese)
        return translated;

    struct Text {
        const char* source;
        const char* simplified;
        const char* traditional;
    };
    static const Text texts[] = {
        {"Today", u8"今天", u8"今天"},
        {"Yesterday", u8"昨天", u8"昨天"},
        {"Past week", u8"过去一周", u8"過去一週"},
        {"Past month", u8"过去一个月", u8"過去一個月"},
        {"Past six months", u8"过去六个月", u8"過去六個月"},
        {"Past year", u8"过去一年", u8"過去一年"},
        {"Clear all", u8"全部清除", u8"全部清除"},
        {"Done", u8"完成", u8"完成"},
        {"Date range", u8"日期范围", u8"日期範圍"},
        {"Choose date range", u8"选择日期范围", u8"選擇日期範圍"},
        {"Select date range", u8"选择日期范围", u8"選擇日期範圍"},
        {"End must not precede start", u8"结束时间不能早于开始时间", u8"結束時間不能早於開始時間"},
        {"Start date", u8"开始日期", u8"開始日期"},
        {"End date", u8"结束日期", u8"結束日期"},
        {"Start time", u8"开始时间", u8"開始時間"},
        {"End time", u8"结束时间", u8"結束時間"},
        {"Accept time", u8"确认时间", u8"確認時間"},
        {"Cancel time", u8"取消时间", u8"取消時間"},
        {"Clear start", u8"清除开始时间", u8"清除開始時間"},
        {"Clear end", u8"清除结束时间", u8"清除結束時間"},
        {"Range bounds", u8"范围边界", u8"範圍邊界"},
        {"Date range calendar", u8"日期范围日历", u8"日期範圍日曆"},
        {"Choose the start date", u8"选择开始日期", u8"選擇開始日期"},
        {"Choose the end date", u8"选择结束日期", u8"選擇結束日期"},
        {"Type a time, or use Up and Down to adjust it", u8"输入时间，或使用上、下方向键调整",
            u8"輸入時間，或使用上、下方向鍵調整"},
        {"Until %1", u8"截至 %1", u8"截至 %1"},
        {"From %1", u8"从 %1 起", u8"從 %1 起"},
        {"Unbounded", u8"不限", u8"不限"},
        {"Start", u8"开始", u8"開始"},
        {"End", u8"结束", u8"結束"},
    };
    const bool traditional = locale.script() == QLocale::TraditionalHanScript;
    for (const auto& text : texts)
        if (translated == QLatin1String(text.source))
            return QString::fromUtf8(traditional ? text.traditional : text.simplified);
    return translated;
}

} // namespace fluent::date_time::detail

#endif // FLUENTQT_COMPONENTS_DATE_TIME_DATETIMERANGEPICKERTEXT_P_H
