#ifndef GZIPLINEREADER_H
#define GZIPLINEREADER_H

#include <QByteArray>
#include <QFile>
#include <QString>

#include <zlib.h>

// Reads a gzip file one line at a time, decompressing as it goes, so a large
// file (Scryfall's bulk data is ~200 MB uncompressed) never sits in memory whole.
class GzipLineReader
{
public:
    explicit GzipLineReader(const QString &path);
    ~GzipLineReader();

    GzipLineReader(const GzipLineReader &) = delete;
    GzipLineReader &operator=(const GzipLineReader &) = delete;

    bool open();

    // Next line, without its line ending. Returns false at the end of the file
    // or on an error; errorString() tells the two apart.
    bool readLine(QByteArray &line);

    QString errorString() const { return m_error; }

    // Progress through the compressed file, for progress bars.
    qint64 compressedSize() const { return m_file.size(); }
    qint64 compressedPosition() const;

private:
    bool fill();

    QFile m_file;
    z_stream m_stream{};
    bool m_streamReady = false;
    bool m_insideMember = false;  // started a gzip member but not reached its end yet
    QByteArray m_input;
    QByteArray m_output;
    QByteArray m_buffer;          // decompressed data not yet returned as lines
    qsizetype m_bufferPos = 0;
    QString m_error;
};

#endif // GZIPLINEREADER_H
