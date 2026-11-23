#pragma once

#include <QList>
#include <QString>

struct Mark {
    QString title;
    qint64 startMs = 0;
};

struct BookFile {
    QString id, path, title;
    qint64 size = 0, durationMs = 0;
    QList<Mark> marks;
};

struct Chapter {
    QString title;
    int track = 0;
    qint64 startInTrack = 0, start = 0, durationMs = 0;
};

struct Book {
    QString id, title, author, narrator, series, year, coverId;
    QList<BookFile> files;
    QList<qint64> starts;
    QList<Chapter> chapters;
    qint64 durationMs = 0;

    // The same rule as the phone: a file's own marks, otherwise the file.
    void finish()
    {
        starts.clear();
        chapters.clear();
        durationMs = 0;
        for (const BookFile &f : files) {
            starts << durationMs;
            durationMs += qMax<qint64>(f.durationMs, 0);
        }
        for (int i = 0; i < files.size(); ++i) {
            const BookFile &f = files[i];
            if (f.marks.size() > 1 || (f.marks.size() == 1 && files.size() == 1)) {
                for (int m = 0; m < f.marks.size(); ++m) {
                    const qint64 end = m + 1 < f.marks.size() ? f.marks[m + 1].startMs : f.durationMs;
                    chapters << Chapter{f.marks[m].title, i, f.marks[m].startMs, starts[i] + f.marks[m].startMs,
                                        qMax<qint64>(end - f.marks[m].startMs, 0)};
                }
            } else {
                chapters << Chapter{f.title, i, 0, starts[i], qMax<qint64>(f.durationMs, 0)};
            }
        }
    }

    int chapterAt(qint64 ms) const
    {
        int found = chapters.isEmpty() ? -1 : 0;
        for (int i = 0; i < chapters.size() && chapters[i].start <= ms; ++i)
            found = i;
        return found;
    }

    std::pair<int, qint64> locate(qint64 ms) const
    {
        if (files.isEmpty())
            return {0, 0};
        ms = qBound<qint64>(0, ms, durationMs);
        int index = 0;
        for (int i = 0; i < starts.size() && starts[i] <= ms; ++i)
            index = i;
        while (index < files.size() - 1 && files[index].durationMs <= 0)
            ++index;
        return {index, qMax<qint64>(ms - starts[index], 0)};
    }
};

struct Position {
    QString path;
    qint64 size = 0, ms = 0;
    bool finished = false;
    double speed = 1;
    qint64 lastPlayed = 0; // on this machine's clock
};

struct Bookmark {
    QString id, path, note;
    qint64 size = 0, ms = 0, created = 0, updated = 0;
    bool deleted = false;
};

inline QString fileKey(const QString &path, qint64 size)
{
    return path + QLatin1Char('\n') + QString::number(size);
}
