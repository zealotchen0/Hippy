let uuid = new Date().getTime();

export default function getUniqueID() {
  // eslint-disable-next-line no-plusplus
  uuid++;
  return `rnmr_${uuid.toString(16)}`;
}
