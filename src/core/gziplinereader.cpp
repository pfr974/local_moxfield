#include "gziplinereader.h"

namespace {
constexpr qsizetype kChunkSize = 256 * 1024;
}

GzipLineReader::GzipLineReader(const QString &path)
    : m_file(path)
{
}

GzipLineReader::~GzipLineReader()
{
    if (m_streamReady)
        inflateEnd(&m_stream);
}

bool GzipLineReader::open()
{
    if (!m_file.open(QIODevice::ReadOnly)) {
        m_error = m_file.errorString();
        return false;
    }
    // 16 + MAX_WBITS tells zlib to expect a gzip header rather than a raw zlib stream.
    if (inflateInit2(&m_stream, 16 + MAX_WBITS) != Z_OK) {
        m_error = QStringLiteral("Could not initialise zlib");
        return false;
    }
    m_streamReady = true;
    m_output.resize(kChunkSize);
    return true;
}

qint64 GzipLineReader::compressedPosition() const
{
    return m_file.pos() - m_stream.avail_in;
}

bool GzipLineReader::readLine(QByteArray &line)
{
    while (true) {
        const qsizetype newline = m_buffer.indexOf('\n', m_bufferPos);
        if (newline >= 0) {
            qsizetype end = newline;
            if (end > m_bufferPos && m_buffer.at(end - 1) == '\r')
                --end;
            line = m_buffer.mid(m_bufferPos, end - m_bufferPos);
            m_bufferPos = newline + 1;
            return true;
        }
        if (!fill()) {
            // A final line with no trailing newline.
            if (m_error.isEmpty() && m_bufferPos < m_buffer.size()) {
                line = m_buffer.mid(m_bufferPos);
                m_bufferPos = m_buffer.size();
                return true;
            }
            return false;
        }
    }
}

// Decompresses more data onto the end of m_buffer. False when there is no more.
bool GzipLineReader::fill()
{
    if (!m_streamReady || !m_error.isEmpty())
        return false;

    m_buffer.remove(0, m_bufferPos);
    m_bufferPos = 0;
    const qsizetype sizeBefore = m_buffer.size();

    while (m_buffer.size() == sizeBefore) {
        if (m_stream.avail_in == 0) {
            m_input = m_file.read(kChunkSize);
            if (m_input.isEmpty()) {
                if (m_file.error() != QFileDevice::NoError)
                    m_error = m_file.errorString();
                else if (m_insideMember)
                    m_error = QStringLiteral("The file is truncated (incomplete download?)");
                return false;
            }
            m_stream.next_in = reinterpret_cast<Bytef *>(m_input.data());
            m_stream.avail_in = static_cast<uInt>(m_input.size());
        }

        m_stream.next_out = reinterpret_cast<Bytef *>(m_output.data());
        m_stream.avail_out = static_cast<uInt>(m_output.size());
        const int result = inflate(&m_stream, Z_NO_FLUSH);

        if (result == Z_STREAM_END) {
            // gzip files may hold several members back to back; carry on with the next one.
            m_insideMember = false;
            inflateReset(&m_stream);
        } else if (result == Z_OK) {
            m_insideMember = true;
        } else {
            m_error = QStringLiteral("Corrupt gzip data: %1")
                          .arg(QString::fromLatin1(m_stream.msg ? m_stream.msg : "unknown error"));
            return false;
        }
        m_buffer.append(m_output.constData(), m_output.size() - m_stream.avail_out);
    }
    return true;
}
