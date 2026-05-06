#include <QFile>
#include "utils/logger.h"
#include "VpnConf.h"

QString VpnConfig::generateWireguardConfig() const {
	return QStringLiteral(
		"[Interface]\n"
		"PrivateKey = %1\n"
		"Address = %2\n"
		"DNS = %3\n"
		"MTU = %4\n"
		"\n"
		"[Peer]\n"
		"PublicKey = %5\n"
		"PresharedKey = %6\n"
		"AllowedIPs = %7\n"
		"PersistentKeepalive = %8\n"
		"Endpoint = %9\n"
	)
		.arg(this->iface.privateKey)
		.arg(this->iface.address)
		.arg(this->iface.dns)
		.arg(this->iface.mtu)
		.arg(this->peer.publicKey)
		.arg(this->peer.presharedKey)
		.arg(this->peer.allowedIPs.join(", "))
		.arg(this->peer.persistentKeepalive)
		.arg(this->peer.endpoint);
};

bool VpnConfig::writeVpnConfig(const QString& filePath) const {
	const QString content = generateWireguardConfig();

	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		spdlog::error("writeVpnConfig: failed to open '{}': {}",
			filePath.toStdString(),
			file.errorString().toStdString());
		return false;
	}

	const QByteArray bytes = content.toUtf8();
	if (file.write(bytes) != bytes.size()) {
		spdlog::error("writeVpnConfig: incomplete write to '{}'", filePath.toStdString());
		file.close();
		return false;
	}

	file.close();
	spdlog::info("writeVpnConfig: wrote {}B to '{}'",
		bytes.size(), filePath.toStdString());
	return true;
}
