/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/MediaReader.h"
#include "Mona/MediaWriter.h"
#include "Mona/Segments.h"
#include "Mona/MediaStream.h"

namespace Mona {


struct MediaFile : virtual Static  {

	struct Reader : MediaStream, virtual Object {
		static unique<MediaFile::Reader> New(Exception& ex, const char* request, Media::Source& source, const Timer& timer, IOFile& io, std::string&& format = "");
	
		Reader(const Path& path, unique<MediaReader>&& pReader, Media::Source& source, const Timer& timer, IOFile& io);
		virtual ~Reader() { stop(); }

		const Path		path;
		IOFile&			io;
		const Timer&	timer;

		const MediaReader* operator->() const { return _pReader.get(); }

	private:
		bool starting(const Parameters& parameters);
		void stopping();

		struct Lost : Media::Base, virtual Object {
			Lost(Media::Type type, UInt32 lost, UInt8 track) : Media::Base(type, Packet::Null(), track), _lost(lost) {} // lost
			operator const UInt32&() { return _lost; }
		private:
			UInt32 _lost;
		};
		struct Decoder : File::Decoder, private Media::Source, virtual Object {
			typedef Event<void()>	ON(Flush);

			Decoder(const Handler& handler, const shared<MediaReader>& pReader, const Path& path, const std::string& name, const shared<std::deque<unique<Media::Base>>>& pMedias) :
				_name(name), _handler(handler), _pReader(pReader), _path(path), _pMedias(pMedias) {}

		private:
			UInt32 decode(shared<Buffer>& pBuffer, bool end) override;

			void writeAudio(const Media::Audio::Tag& tag, const Packet& packet, UInt8 track = 1) { if(!_mediaTimeGotten) _mediaTimeGotten = !tag.isConfig; writeMedia<Media::Audio>(tag, packet, track); }
			void writeVideo(const Media::Video::Tag& tag, const Packet& packet, UInt8 track = 1) { if (!_mediaTimeGotten) _mediaTimeGotten = tag.frame != Media::Video::FRAME_CONFIG; writeMedia<Media::Video>(tag, packet, track); }
			void writeData(Media::Data::Type type, const Packet& packet, UInt8 track = 0) { writeMedia<Media::Data>(type, packet, track); }
			void addProperties(UInt8 track, Media::Data::Type type, const Packet& packet) { writeMedia<Media::Data>(type, packet, track, true); }
			void reportLost(Media::Type type, UInt32 lost, UInt8 track = 0) { writeMedia<Lost>(type, lost, track); }
			void flush() { } // do nothing on source flush, to do the flush in end of file reading!
			void reset() { _pMedias->emplace_back(); }

			template <typename MediaType, typename ...Args>
			void writeMedia(Args&&... args) {
				_pMedias->emplace_back();
				_pMedias->back().set<MediaType>(std::forward<Args>(args)...);
			}
			
			shared<MediaReader>		_pReader;
			std::string				_name;
			const Handler&			_handler;
			Path					_path;
			bool					_mediaTimeGotten;
			
			shared<std::deque<unique<Media::Base>>> _pMedias;
		};

		Decoder::OnFlush		_onFlush;

		File::OnError			_onFileError;

		shared<MediaReader>		_pReader;
		shared<File>			_pFile;
		shared<Decoder>			_pDecoder;

		Timer::OnTimer			_onTimer;
		Time					_realTime;
		shared<std::deque<unique<Media::Base>>> _pMedias;
	};



	struct Writer : Media::Target, MediaStream, virtual Object {
		static unique<MediaFile::Writer> New(Exception& ex, const char* request, IOFile& io, std::string&& format = "");

		Writer(const Path& path, unique<MediaWriter>&& pWriter, IOFile& io);
		virtual ~Writer() { stop(); }

		const Path	path;
		IOFile&		io;
		UInt64		queueing() const { return _pFile ? _pFile->queueing() : 0; }
		bool		segmented() const { return _pPlaylistWriter.operator bool(); }
		UInt32		segments() const { return _segments; }

		const MediaWriter* operator->() const { return _pWriter.get(); }

		bool beginMedia(const std::string& name);
		bool writeProperties(const Media::Properties& properties);
		bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Audio>>(tag, packet, track); }
		bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Video>>(tag, packet, track); }
		bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) { return write<MediaWrite<Media::Data>>(type, packet, track); }
		bool endMedia();

	private:
		bool starting(const Parameters& parameters);
		void stopping();

		template<typename WriteType, typename ...Args>
		bool write(Args&&... args) {
			if (!_pFile)
				return false; // Stream not begin or has failed!
			io.threadPool.queue<WriteType>(_writeTrack, _pFile, std::forward<Args>(args)...);
			return true;
		}

		struct File : FileWriter, Playlist, virtual Object {
			NULLABLE(!FileWriter::operator bool())

			File(const std::string& name, const Path& path, const shared<MediaWriter>& pWriter, const shared<Playlist::Writer>& pPlaylistWriter, UInt32 segments, UInt16 duration, IOFile& io);
			~File();

			File& open(bool append = true);
			bool segmented() const { return _pPlaylistWriter.operator bool(); }

			MediaWriter*	operator->() { return _pWriter.get(); }
			MediaWriter&	operator*() { return *_pWriter; }

			const std::string				name;
			MediaWriter::OnWrite			onWrite;
			UInt32							segments;
		private:
			const shared<Playlist::Writer>	_pPlaylistWriter;
			shared<MediaWriter>				_pWriter;
		};


		struct Write : Runner, virtual Object {
			Write(const shared<File>& pFile) : _pFile(pFile), Runner("MediaFileWrite") {}
		private:
			virtual void process(Exception& ex, File& file) = 0;
			bool run(Exception& ex) { process(ex, *_pFile); return true; }
			shared<File>		_pFile;
		};

		struct Begin : Write, virtual Object {
			Begin(const shared<File>& pFile, bool append) : Write(pFile), append(append) {}
			const bool	append;
			void process(Exception& ex, File& file);
		};

		struct ChangeDuration : Write, virtual Object {
			ChangeDuration(const shared<File>& pFile, UInt16 duration) : Write(pFile), duration(duration) {}
			const UInt16 duration;
			void process(Exception& ex, File& file) { if (typeid(*file) == typeid(Segments::Writer)) ((Segments::Writer&)*file).setDuration(duration); }
		};
		struct ChangeSegments : Write, virtual Object {
			ChangeSegments(const shared<File>& pFile, UInt32 segments) : Write(pFile), segments(segments) {}
			const UInt32 segments;
			void process(Exception& ex, File& file) {
				if(segments)
					Segments::Clear(file, segments, file.io);
			}
		};

		template<typename MediaType>
		struct MediaWrite : Write, MediaType, virtual Object {
			template<typename ...Args>
			MediaWrite(const shared<File>& pFile, Args&&... args) : Write(pFile), MediaType(std::forward<Args>(args)...) {}
			void process(Exception& ex, File& file) { file->writeMedia(self, file.onWrite); }
		};
		struct End : Write, virtual Object {
			End(const shared<File>& pFile) : Write(pFile) {}
			void process(Exception& ex, File& file) { file->endMedia(file.onWrite); }
		};

		shared<File>			 _pFile;
		shared<MediaWriter>		 _pWriter;
		UInt16					 _writeTrack;
		bool					 _append;
		UInt16					 _duration;
		UInt32					 _segments;
		shared<Playlist::Writer> _pPlaylistWriter;
	};
};

} // namespace Mona
