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
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QBuffer>
#include "UDJServerConnection.hpp"
#include "JSONHelper.hpp"

Q_DECLARE_METATYPE(std::vector<UDJ::client_request_id_t>)
namespace UDJ{

UDJServerConnection::UDJServerConnection(QObject *parent):QObject(parent),
  isLoggedIn(false)
{
  netAccessManager = new QNetworkAccessManager(this);
  connect(netAccessManager, SIGNAL(finished(QNetworkReply*)),
    this, SLOT(recievedReply(QNetworkReply*)));
}

void UDJServerConnection::startConnection(
  const QString& username,
  const QString& password
)
{
  authenticate(username, password);
}

void UDJServerConnection::prepareJSONRequest(QNetworkRequest &request){
  request.setHeader(QNetworkRequest::ContentTypeHeader, "text/json");
  request.setRawHeader(getTicketHeaderName(), ticket_hash);
}

void UDJServerConnection::addLibSongOnServer(
	const QString& songName,
	const QString& artistName,
	const QString& albumName,
  const int duration,
	const library_song_id_t hostId)
{
  if(!isLoggedIn){
    return;
  }
  bool success = true;

  lib_song_t songToAdd = {hostId, songName, artistName, albumName, duration};

  const QByteArray songJSON = JSONHelper::getJSONForLibAdd(
    songToAdd,
    success);
  QNetworkRequest addSongRequest(getLibAddSongUrl());
  prepareJSONRequest(addSongRequest);
  netAccessManager->put(addSongRequest, songJSON);
}

void UDJServerConnection::authenticate(
  const QString& username, 
  const QString& password)
{
  QNetworkRequest authRequest(getAuthUrl());
  authRequest.setRawHeader(
    getAPIVersionHeaderName(),
    getAPIVersion());
  QString data("username="+username+"&password="+password);
  QBuffer *dataBuffer = new QBuffer();
  dataBuffer->setData(data.toUtf8());
  QNetworkReply *reply = netAccessManager->post(authRequest, dataBuffer);
  dataBuffer->setParent(reply);
}
 
void UDJServerConnection::createEvent(
  const QString& partyName,
  const QString& password)
{
  QNetworkRequest createEventRequest(getCreateEventUrl());
  prepareJSONRequest(createEventRequest);
  const QByteArray partyJSON = JSONHelper::getCreateEventJSON(
    partyName, password);
  netAccessManager->put(createEventRequest, partyJSON);
}

void UDJServerConnection::endEvent(){
  QNetworkRequest endEventRequest(getEndEventUrl());
  endEventRequest.setRawHeader(getTicketHeaderName(), ticket_hash);
  netAccessManager->deleteResource(endEventRequest);
}

void UDJServerConnection::addSongToAvailableSongs(library_song_id_t songToAdd){
  std::vector<library_song_id_t> toAddVector(1, songToAdd);
  addSongsToAvailableSongs(toAddVector);
}

void UDJServerConnection::addSongsToAvailableSongs(
  const std::vector<library_song_id_t>& songsToAdd)
{
  if(songsToAdd.size() <= 0){
    return;
  }
  QNetworkRequest addSongToAvailableRequest(getAddSongToAvailableUrl());
  prepareJSONRequest(addSongToAvailableRequest);
  const QByteArray songsAddJSON = JSONHelper::getAddToAvailableJSON(songsToAdd);
  netAccessManager->put(addSongToAvailableRequest, songsAddJSON);
}

void UDJServerConnection::getActivePlaylist(){
  QNetworkRequest getActivePlaylistRequest(getActivePlaylistUrl());
  getActivePlaylistRequest.setRawHeader(getTicketHeaderName(), ticket_hash);
  netAccessManager->get(getActivePlaylistRequest);
}

void UDJServerConnection::addSongToActivePlaylist(
  client_request_id_t requestId, 
  library_song_id_t songId)
{
  std::vector<client_request_id_t> requestIds(1, requestId);
  std::vector<library_song_id_t> songIds(1, songId);
  addSongsToActivePlaylist(requestIds, songIds);
}

void UDJServerConnection::addSongsToActivePlaylist(
  const std::vector<client_request_id_t>& requestIds, 
  const std::vector<library_song_id_t>& songIds)
{
  QNetworkRequest add2ActivePlaylistRequest(getActivePlaylistAddUrl());
  prepareJSONRequest(add2ActivePlaylistRequest);
  bool success;
  const QByteArray songsAddJSON = JSONHelper::getAddToActiveJSON(
    requestIds,
    songIds,
    success);
  if(!success){
    //TODO handle error
    DEBUG_MESSAGE("Error serializing active playlist addition reuqest")
    return;
  }
  QNetworkReply *reply = 
    netAccessManager->put(add2ActivePlaylistRequest, songsAddJSON);
  reply->setProperty(getActivePlaylistRequestIdsPropertyName(),
    QVariant::fromValue<std::vector<client_request_id_t> >(requestIds));
}

void UDJServerConnection::recievedReply(QNetworkReply *reply){
  if(reply->request().url().path() == getAuthUrl().path()){
    handleAuthReply(reply);
  }
  else if(reply->request().url().path() == getLibAddSongUrl().path()){
    handleAddLibSongsReply(reply);
  }
  else if(reply->request().url().path() == getCreateEventUrl().path()){
    handleCreateEventReply(reply);
  }
  else if(reply->request().url().path() == getEndEventUrl().path()){
    handleEndEventReply(reply);
  }
  else if(reply->request().url().path() == getAddSongToAvailableUrl().path()){
    handleAddAvailableSongReply(reply);
  }
  else if(reply->request().url().path() == getActivePlaylistUrl().path()){
    handleRecievedActivePlaylist(reply);
  }
  else if(reply->request().url().path() == getActivePlaylistAddUrl().path()){
    handleRecievedActivePlaylistAdd(reply);
  }
  else{
    std::cerr << "Recieved unknown response" << std::endl;
  }
  reply->deleteLater();
}

void UDJServerConnection::handleAuthReply(QNetworkReply* reply){
  if(
    reply->error() == QNetworkReply::NoError &&
    reply->hasRawHeader(getTicketHeaderName()) &&
    reply->hasRawHeader(getUserIdHeaderName()))
  {
    setLoggedIn(
      reply->rawHeader(getTicketHeaderName()),
      reply->rawHeader(getUserIdHeaderName())
    );
    emit connectionEstablished();
  }
  else{
    QString error = tr("Unable to connect to server: error ") + 
     QString::number(reply->error());
    emit unableToConnect(error);
  }
}

void UDJServerConnection::setLoggedIn(QByteArray ticket, QByteArray userId){
  ticket_hash = ticket;
  user_id = QString(userId).toLong();
  timeTicketIssued = QDateTime::currentDateTime();
  isLoggedIn = true;
}

void UDJServerConnection::handleAddLibSongsReply(QNetworkReply *reply){
  const std::vector<library_song_id_t> updatedIds =   
    JSONHelper::getUpdatedLibIds(reply);
  emit songsAddedToLibOnServer(updatedIds);
}

void UDJServerConnection::handleAddAvailableSongReply(QNetworkReply *reply){
  if(reply->error() == QNetworkReply::NoError){
    std::vector<library_song_id_t> addedIds = 
      JSONHelper::getAddedAvailableSongs(reply);
    emit songsAddedToAvailableMusic(addedIds); 
  }
}

void UDJServerConnection::handleCreateEventReply(QNetworkReply *reply){
  //TODO
  // Handle if a 409 response is returned
  if(reply->error() != QNetworkReply::NoError){
    emit eventCreationFailed("Failed to create event");
    QByteArray errormsg = reply->readAll();
    DEBUG_MESSAGE(QString(errormsg).toStdString())
    return;
  }
  //TODO handle bad json resturned from the server.
  isHostingEvent =true;
  eventId = JSONHelper::getEventId(reply);
  emit eventCreated();
}

void UDJServerConnection::handleEndEventReply(QNetworkReply *reply){
  if(reply->error() != QNetworkReply::NoError){
    emit eventEndingFailed(tr("Failed to end event") + 
      QString::number(reply->error()) );
    return;
  }
  emit eventEnded();
}

void UDJServerConnection::handleRecievedActivePlaylist(QNetworkReply *reply){
  if(reply->error() == QNetworkReply::NoError){
    emit newActivePlaylist(JSONHelper::getActivePlaylistFromJSON(reply));
  }
}

void UDJServerConnection::handleRecievedActivePlaylistAdd(QNetworkReply *reply){
  if(reply->error() == QNetworkReply::NoError){
    QVariant property = reply->property(
      getActivePlaylistRequestIdsPropertyName());
    std::vector<client_request_id_t> requestedIds = 
      property.value<std::vector<client_request_id_t> >();
    emit songsAddedToActivePlaylist(requestedIds);
  }
  else{
    QByteArray error = reply->readAll();
    DEBUG_MESSAGE(QString(error).toStdString())
  }
}


QUrl UDJServerConnection::getLibAddSongUrl() const{
  return QUrl(getServerUrlPath() + "users/" + QString::number(user_id) +
    "/library/songs");
}

QUrl UDJServerConnection::getLibDeleteAllUrl() const{
  return QUrl(getServerUrlPath() + "users/" + QString::number(user_id) +
    "/library");
}

QUrl UDJServerConnection::getEndEventUrl() const{
  return QUrl(getServerUrlPath() + "events/" + QString::number(eventId));
}

QUrl UDJServerConnection::getAddSongToAvailableUrl() const{
  return QUrl(getServerUrlPath() + "events/" + QString::number(eventId) +
    "/available_music");
}

QUrl UDJServerConnection::getActivePlaylistUrl() const{
  return QUrl(getServerUrlPath() + "events/" + QString::number(eventId) +
    "/active_playlist");
}

QUrl UDJServerConnection::getActivePlaylistAddUrl() const{
  return QUrl(getServerUrlPath() + "events/" + QString::number(eventId) +
    "/active_playlist/songs");
}

void UDJServerConnection::clearMyLibrary(){
//TODO implement this
}


}//end namespace
