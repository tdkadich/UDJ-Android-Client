/**
 * Copyright 2011 Kurtis L. Nusbaum
 * 
 * This file is part of UDJ.
 * 
 * UDJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * UDJ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with UDJ.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef MUSIC_FINDER_HPP
#define MUSIC_FINDER_HPP

#include <QDir>
#include "phonon/mediasource.h"


namespace UDJ{


class MusicFinder{
public:
  static QList<Phonon::MediaSource> findMusicInDir(const QString& musicDir);
private:
  static bool isMusicFile(const QFileInfo& file);
  static const QRegExp& getMusicFileMatcher(){
    //static const QRegExp matcher("*.mp3|*.m4a|*.ogg|*.wav", Qt::CaseSensitive, QRegExp::Wildcard);
    static const QRegExp matcher("*.mp3", Qt::CaseSensitive, QRegExp::Wildcard);
    return matcher;
  }
}; 


} //end namespace
#endif //MUSIC_FINDER_HPP